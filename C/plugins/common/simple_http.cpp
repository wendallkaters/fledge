/*
 * Fledge HTTP Sender implementation using the
 * Simple Web Seerver HTTP library.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto, Mark Riddoch
 */
#include <simple_http.h>
#include <thread>
#include <logger.h>

#define VERBOSE_LOG	0

using namespace std;

// Using https://github.com/eidheim/Simple-Web-Server
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

/**
 * Constructor: host:port, connect_timeout, request_timeout
 */
SimpleHttp::SimpleHttp(const string& host_port,
		       unsigned int connect_timeout,
		       unsigned int request_timeout,
		       unsigned int retry_sleep_Time,
		       unsigned int max_retry) :
		       HttpSender(), m_host_port(host_port),
		       m_retry_sleep_time(retry_sleep_Time),
		       m_max_retry (max_retry)
{
	m_sender = new HttpClient(host_port);
	m_sender->config.timeout = (time_t)request_timeout;
	m_sender->config.timeout_connect = (time_t)connect_timeout;
}

/**
 * Destructor
 */
SimpleHttp::~SimpleHttp()
{
	delete m_sender;
}

/**
 * Send data
 *
 * @param method    The HTTP method (GET, POST, ...)
 * @param path      The URL path
 * @param headers   The optional headers to send
 * @param payload   The optional data payload (for POST, PUT)
 * @return          The HTTP code on success or 0 on execptions
 */
int SimpleHttp::sendRequest(const string& method,
			    const string& path,
			    const vector<pair<string, string>>& headers,
			    const string& payload)
{
	SimpleWeb::CaseInsensitiveMultimap header;

	// Add Fledge UserAgent
	header.emplace("User-Agent", HTTP_SENDER_USER_AGENT);
	header.emplace("Content-Type", "application/json");

	// Add custom headers
	for (auto it = headers.begin(); it != headers.end(); ++it)
	{
		header.emplace((*it).first, (*it).second);
	}

	// Handle basic authentication
	if (m_authMethod == "b")
		header.emplace("Authorization", "Basic " + m_authBasicCredentials);

	string retCode;
	string response;
	int http_code;

	bool retry = false;
	int  retry_count = 1;
	int  sleep_time = m_retry_sleep_time;

	enum exceptionType
	{
	    none, typeBadRequest, typeException
	};

	exceptionType exception_raised;
	string exception_message;

	do
	{
		try
		{
			exception_raised = none;
			http_code = 0;

			// Call HTTPS method
			auto res = m_sender->request(method, path, payload, header);

			retCode = res->status_code;
			response = res->content.string();

			// In same cases the response is an empty string
			// and retCode contains code and the description
			if (response.compare("") == 0)
				response = res->status_code;

			http_code = atoi(retCode.c_str());

		}
		catch (BadRequest &ex)
		{
			exception_raised = typeBadRequest;
			exception_message = ex.what();

		}
		catch (exception &ex)
		{
			exception_raised = typeException;
			exception_message = "Failed to send data: ";
			exception_message.append(ex.what());
		}

		if (exception_raised == none &&
		    ((http_code >= 200) && (http_code <= 399)))
		{
			retry = false;
#if VERBOSE_LOG
			Logger::getLogger()->info("HTTP sendRequest succeeded : retry count |%d| HTTP code |%d| message |%s|",
						  retry_count,
						  http_code,
						  payload.c_str());
#endif
		}
		else
		{
#if VERBOSE_LOG
			if (exception_raised)
			{
				Logger::getLogger()->error(
					"HTTP sendRequest : retry count |%d| error |%s| message |%s|",
					retry_count,
					exception_message.c_str(),
					payload.c_str());

			}
			else
			{
				Logger::getLogger()->error(
					"HTTP sendRequest : retry count |%d| HTTP code |%d| HTTP error |%s| message |%s|",
					retry_count,
					http_code,
					response.c_str(),
					payload.c_str());
			}
#endif

			if (retry_count < m_max_retry)
			{
				this_thread::sleep_for(chrono::seconds(sleep_time));

				retry = true;
				sleep_time *= 2;
				retry_count++;
			}
			else
			{
				retry = false;
			}
		}

	} while (retry);

	// Check if an error should be raised
	if (exception_raised == none)
	{
		// If 400 Bad Request, throw BadRequest exception
		if (http_code == 400)
		{
			throw BadRequest(response);
		}
		else if (http_code >= 401)
		{
			std::stringstream error_message;
			error_message << "HTTP code |" << to_string(http_code) << "| HTTP error |" << response << "|";

			throw runtime_error(error_message.str());
		}
	}
	else
	{
		if (exception_raised == typeBadRequest)
		{
			throw BadRequest(exception_message);
		}
		else if (exception_raised == typeException)
		{
			throw runtime_error(exception_message);
		}
	}

	return http_code;
}
