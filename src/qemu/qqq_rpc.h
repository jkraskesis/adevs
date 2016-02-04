#ifndef _QQQ_RPC_H_
#define _QQQ_RPC_H_
#include "adevs_qemu.h"
#include <unistd.h>
#include <exception>
#include <string>
#include <vector>

/**
  * Basic interface to any emulated computer system.
  */
class Basic_Machine
{
	public:
		virtual int run(int usecs) = 0;
		virtual bool is_alive() = 0;
		virtual ~Basic_Machine(){};
};

/**
 * This class encapsulates a QEMU Machine.
 */
class QEMU_Machine:
	public Basic_Machine
{
	public:
		/**
		 * Instantiate a machine using the supplied command line
		 * arguments to fork its process. 
		 */
		QEMU_Machine(const char* executable, const std::vector<std::string>& arguments);
		/**
		 * Instruct the machine to execute for at most usec
		 * microseconds of simulated time and then return.
		 * The return value is the number of microseconds
		 * that actually advanced. 
		 */
		int run(int usecs);
		/**
		 * Returns true if qemu is still executing and false if it
		 * has terminated.
		 */
		bool is_alive();
		/**
		 * Shut the machine down now.
		 */
		virtual ~QEMU_Machine();

	private:
		unsigned pid;
		// Pipe for talking to qemu
		int read_fd, write_fd;

		void write_mem_value(int val);
		int read_mem_value();	
};

/**
 * This class encapsulates a uCsim Machine.
 */
class uCsim_Machine:
	public Basic_Machine,
	public adevs::ComputerMemoryAccess
{
	public:
		/**
		 * Instantiate a machine using the supplied command line
		 * arguments to fork its process. 
		 */
		uCsim_Machine(
				const char* executable,
				const std::vector<std::string>& arguments);
		/**
		 * Instruct the machine to execute for at most usec
		 * microseconds of simulated time and then return.
		 * The return value is the number of microseconds
		 * that actually advanced. 
		 */
		int run(int usecs);
		/**
		 * Returns true if qemu is still executing and false if it
		 * has terminated.
		 */
		bool is_alive();
		/**
		 * Shut the machine down now.
		 */
		virtual ~uCsim_Machine();

		unsigned read_mem(unsigned addr);
		void write_mem(unsigned addr, unsigned data);

	private:
		double elapsed_secs;
		unsigned pid;
		int read_pipe[2];
		int write_pipe[2];
		static const double mega_hz;
		static const double instrs_per_usec;
		pthread_mutex_t mtx;
		char run_buf[1000];
		char write_buf[1000];
		char read_buf[1000];

		void scan_to_prompt(char* scan_buf);
};

/**
 * Exceptions thrown when there are problems with the emulator.
 */
class qemu_exception:
	public std::exception
{
	public:
		qemu_exception(const char* err_msg) throw():
			std::exception(),
			err_msg(err_msg)
		{
		}
		qemu_exception(const std::exception& other) throw():
			std::exception(),
			err_msg(other.what())
		{
		}
		exception& operator=(const exception& other) throw()
		{
			err_msg = other.what();
			return *this;
		}
		const char* what() const throw()
		{
			return err_msg.c_str();
		}
		~qemu_exception() throw(){}
	private:
		std::string err_msg;
};

#endif
