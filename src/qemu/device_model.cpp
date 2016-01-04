#include "adevs_qemu.h"
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>

static void* pthread_write_func(void* data)
{
	while (!(static_cast<adevs::QemuDeviceModel*>(data)->_is_done_with_init));
	static_cast<adevs::QemuDeviceModel*>(data)->write_loop();
	return NULL;
}

static void* pthread_read_func(void* data)
{
	static_cast<adevs::QemuDeviceModel*>(data)->init_func();
	static_cast<adevs::QemuDeviceModel*>(data)->read_loop();
	return NULL;
}

void adevs::QemuDeviceModel::init_func()
{
	initialize_io_structures();
	_is_done_with_init = true;
}

void adevs::QemuDeviceModel::read_loop()
{
	adevs::QemuDeviceModel::io_buffer* buf;
	// read should return NULL when the file descriptor is closed
	while ((buf = this->read()) != NULL)
	{
		pthread_mutex_lock(&read_lock);
		read_q.push_back(buf);
		pthread_mutex_unlock(&read_lock);
	}
}

void adevs::QemuDeviceModel::write_loop()
{
	adevs::QemuDeviceModel::io_buffer* buf; 
	while (true)
	{
		pthread_mutex_lock(&write_lock);
		while (write_q.empty() && running)
		{
			pthread_cond_wait(&write_cond,&write_lock);
		}
		if (!running) 
		{
			pthread_mutex_unlock(&write_lock);
			return;
		}
		buf = write_q.front();
		write_q.pop_front();
		pthread_mutex_unlock(&write_lock);
		this->write(buf->get_data(),buf->get_size());
		delete buf;
	}
}

void adevs::QemuDeviceModel::start()
{
	_is_done_with_init = false;
	/**
	 * These threads have high priority because getting data into and out of qemu
	 * should happen as soon as possible to avoid I/O spanning a time step.
	 */
	for (int i = 0; i < 2; i++)
	{
		pthread_attr_init(&io_sched_attr[i]);
		pthread_attr_setinheritsched(&io_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&io_sched_attr[i],SCHED_OTHER);
		fifo_param[i].sched_priority = sched_get_priority_max(SCHED_OTHER);
		pthread_attr_setschedparam(&io_sched_attr[i],&fifo_param[i]);
	}
	pthread_create(&read_thread,&io_sched_attr[0],pthread_read_func,(void*)this);
	pthread_create(&write_thread,&io_sched_attr[1],pthread_write_func,(void*)this);
}

adevs::QemuDeviceModel::QemuDeviceModel():
	running(true)
{
	pthread_mutex_init(&read_lock,NULL);
	pthread_mutex_init(&write_lock,NULL);
	pthread_cond_init(&write_cond,NULL);
}

int adevs::QemuDeviceModel::num_bytes_to_read()
{
	int size = 0;
	pthread_mutex_lock(&read_lock);
	if (!read_q.empty())
		size = read_q.front()->get_size();
	pthread_mutex_unlock(&read_lock);
	return size;
}

void adevs::QemuDeviceModel::write_bytes(void* data, int num_bytes)
{
	io_buffer* buf = new io_buffer(num_bytes);
	memcpy(buf->get_data(),data,num_bytes);
	pthread_mutex_lock(&write_lock);
	write_q.push_back(buf);
	pthread_cond_signal(&write_cond);
	pthread_mutex_unlock(&write_lock);
}

void adevs::QemuDeviceModel::read_bytes(void* data)
{
	adevs::QemuDeviceModel::io_buffer* buf;
	pthread_mutex_lock(&read_lock);
	buf = read_q.front();
	read_q.pop_front();
	pthread_mutex_unlock(&read_lock);
	memcpy(data,buf->get_data(),buf->get_size());
	delete buf;
}

adevs::QemuDeviceModel::~QemuDeviceModel()
{
	// Tell the write thread to stop
	pthread_mutex_lock(&write_lock);
	running = false;
	pthread_cond_signal(&write_cond);
	pthread_mutex_unlock(&write_lock);
	// Wait for both threads to terminate
	pthread_join(read_thread,NULL);
	pthread_join(write_thread,NULL);
	// Clean up
	pthread_attr_destroy(&io_sched_attr[0]);
	pthread_attr_destroy(&io_sched_attr[1]);
	pthread_mutex_destroy(&read_lock);
	pthread_mutex_destroy(&read_lock);
	pthread_cond_destroy(&write_cond);
	while (!read_q.empty())
	{
		delete read_q.front();
		read_q.pop_front();
	}
	while (!write_q.empty())
	{
		delete write_q.front();
		write_q.pop_front();
	}
}

