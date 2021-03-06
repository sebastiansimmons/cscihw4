
#include "cache.hh"
// Using some code from the example at https://www.boost.org/doc/libs/1_72_0/libs/beast/example/http/client/sync/http_client_sync.cpp
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <charconv>
#include <string.h>
#include <memory>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
namespace pt = boost::property_tree;
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>


class Cache::Impl {
  public:
    Impl(std::string host, std::string port): host_(host), port_(port), version_(11), resolver_(ioc_), stream_(ioc_) {
    	auto const results = (resolver_.resolve(host_, port_));
        stream_.connect(results);
        req_.version(version_);
        req_.set(http::field::host, host_);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    }
    
    ~Impl() {
    	// Gracefully close the socket
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    void set(key_type key, Cache::val_type val, Cache::size_type) {
    	auto const target = "/"+ key + "/" + val;
    	send_request(http::verb::put, target);

    	return;
        
    }
    Cache::val_type get(key_type key, Cache::size_type& val_size) const {

    	auto const target = "/"+ key;
    	
    	send_request(http::verb::get, target);
    	
    	if(res_.result() == http::status::found){
	        // copybuffer to string
	        std::string body = res_.body();

			// Create a stringstream of the response so the property tree can interpret it as json
			std::stringstream ss;
			ss << body << std::endl << "\0";

			boost::property_tree::ptree pt;
			boost::property_tree::read_json(ss, pt);

			auto value = pt.get<std::string>("value");
			
	        char *ret_val = new char[value.length() + 1];
	        strncpy ( ret_val, value.c_str(),value.length() + 1);
	        val_size = value.length() + 1;

	    	return ret_val;
	    } else {
	    	return nullptr;
	    }
    }
    bool del(key_type key) {

    	auto const target = "/"+ key;
    	send_request(http::verb::delete_, target);

    	if(res_.result() == http::status::ok){
    		return true;
    	} else {
    		return false;
    	}

    }
    Cache::size_type space_used() const {
    	auto const target = "/";
    	
    	send_request(http::verb::head, target);

        auto strv_space_used = res_["Space-Used"];

        Cache::size_type space_used;
        std::from_chars(strv_space_used.data(), strv_space_used.data() + strv_space_used.size(), space_used);

        return space_used;

    }
    void reset() {
    	auto const target = "/reset";
    	send_request(http::verb::post, target);
    	return;
    }
  private:
  	std::string host_;
  	std::string port_;
  	int version_;
  	net::io_context ioc_;
  	tcp::resolver resolver_;
    mutable beast::tcp_stream stream_;
    mutable beast::flat_buffer buffer_; // (Must persist between reads)
    mutable http::request<http::empty_body> req_;
    mutable http::response<http::string_body> res_;

  	void send_request(http::verb method, std::string target) const {
  		res_ = http::response<http::string_body>();
    	
    	// Set up an HTTP GET request message
        req_.method(method);
        req_.target(target);

        //std::cout << "Request: " << req_ << std::endl;

        // Send the HTTP request to the remote host
        http::write(stream_, req_);

        // Receive the HTTP response
        http::read(stream_, buffer_, res_);

        //std::cout << res_ << std::endl;
        return;
    }
};


// Ape the given Fridge example
Cache::Cache(std::string host, std::string port) : pImpl_(new Impl(host, port)) {}

Cache::~Cache() {}

void Cache::set(key_type key, val_type val, Cache::size_type size){
    pImpl_->set(key, val, size);
}

Cache::val_type Cache::get(key_type key, Cache::size_type& val_size) const {
    return pImpl_->get(key, val_size);
}

bool Cache::del(key_type key) {
    return pImpl_->del(key);
}

Cache::size_type Cache::space_used() const {
    return pImpl_->space_used();
}

void Cache::reset() {
    pImpl_->reset();
}
