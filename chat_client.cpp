//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <stdlib.h>
#include <stdio.h>


#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "chat_message.hpp"


#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "rs232.h"

using boost::asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

int cport_nr;

class chat_client
{
public:
  chat_client(boost::asio::io_context& io_context,
      const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context)
  {
    do_connect(endpoints);
  }

  void write(const chat_message& msg)
  {
    boost::asio::post(io_context_,
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

private:
  void do_connect(const tcp::resolver::results_type& endpoints)
  {
    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, tcp::endpoint)
        {
          if (!ec)
          {
            do_read_header();
          }
        });
  }

  void do_read_header()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_read_body()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
	    std::string str( read_msg_.body(), read_msg_.body_length() );

            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << std::flush; 
	    RS232_cputs(cport_nr, str.data() );
            do_read_header();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write()
  {
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

private:
  boost::asio::io_context& io_context_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 4)
    {
      std::cerr << "Usage: chat_client <host> <port> <serial port#> <baud>" << std::endl

		<< "0	ttyS0	COM1" << std::endl  
		<< "1	ttyS1	COM2" << std::endl  
		<< "2	ttyS2	COM3" << std::endl  
		<< "3	ttyS3	COM4" << std::endl  
		<< "4	ttyS4	COM5" << std::endl  
		<< "5	ttyS5	COM6" << std::endl  
		<< "6	ttyS6	COM7" << std::endl  
		<< "7	ttyS7	COM8" << std::endl  
		<< "8	ttyS8	COM9" << std::endl  
		<< "9	ttyS9	COM10" << std::endl  
		<< "10	ttyS10	COM11" << std::endl  
		<< "11	ttyS11	COM12" << std::endl  
		<< "12	ttyS12	COM13" << std::endl  
		<< "13	ttyS13	COM14" << std::endl  
		<< "14	ttyS14	COM15" << std::endl  
		<< "15	ttyS15	COM16" << std::endl  
		<< "16	ttyUSB0	COM17" << std::endl  
		<< "17	ttyUSB1	COM18" << std::endl  
		<< "18	ttyUSB2	COM19" << std::endl  
		<< "19	ttyUSB3	COM20" << std::endl  
		<< "20	ttyUSB4	COM21" << std::endl  
		<< "21	ttyUSB5	COM22" << std::endl  
		<< "22	ttyAMA0	COM23" << std::endl  
		<< "23	ttyAMA1	COM24" << std::endl  
		<< "24	ttyACM0	COM25" << std::endl  
		<< "25	ttyACM1	COM26" << std::endl ; 


      return 1;
    }

    boost::asio::io_context io_context;

    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);
    cport_nr = atoi( argv[3] );

    chat_client c(io_context, endpoints);

    std::thread t([&io_context](){ io_context.run(); });

    char line[chat_message::max_body_length + 1];




  int i, n,
      bdrate=11500;
  
  if(argc >= 5 )
	  bdrate=atoi(argv[4]);       /* 9600 baud */

  unsigned char buf[4096];

  char mode[]={'8','N','1',0};


  if(RS232_OpenComport(cport_nr, bdrate, mode, 0))
  {
    printf("Can not open comport\n");

    return(0);
  }





    //while (std::cin.getline(line, chat_message::max_body_length + 1))
    while(1)
    {
      n = RS232_PollComport(cport_nr, buf, 4095);

          if(n > 0)
    {
      buf[n] = 0;   /* always put a "null" at the end of a string! */

      for(i=0; i < n; i++)
      {
        if(buf[i] < 32 && buf[i] != 13 && buf[i] != 10 )  /* replace unreadable control-codes by dots */
        {
          buf[i] = '*';
        }
      }

      //printf("received %i bytes: %s\n", n, (char *)buf);
      //printf("%s", (char *)buf);
      //fflush( stdout );


      chat_message msg;
      msg.body_length(std::strlen((char*)buf));
      std::memcpy(msg.body(), (char*)buf, msg.body_length());
      msg.encode_header();
      c.write(msg);
      fflush( stdout );

    }
    }
    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}


