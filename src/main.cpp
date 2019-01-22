/*
   Mathieu Stefani, 13 février 2016

   Example of an hello world server
*/


#include "pistache/endpoint.h"
#include "include/converter.h"

using namespace Pistache;

class HelloHandler : public Http::Handler
{
public:
    HTTP_PROTOTYPE(HelloHandler)

    void onRequest(const Http::Request& request, Http::ResponseWriter response)
	{
		using namespace Http;

		// Check if GET method and "convert" command is used.
		if (request.method() != Http::Method::Get ||
			request.resource() != "/convert")
		{
			response.send(Pistache::Http::Code::Not_Implemented, "Unknown method or command used!");
			return;
		}

		// Fetch parameters.
		std::string from, to, value;
		for (auto it = request.query().parameters_begin(); it != request.query().parameters_end(); ++it)
		{
			auto parameter = *it;
			auto[key, key_value] = parameter;
			if (key == "from")
				from = key_value;
			else if (key == "to")
				to = key_value;
			else if (key == "value")
				value = key_value;
		}

		// Convert value to floating.
		float from_value;
		try
		{
			from_value = stof(value);
		}
		catch (std::exception &)
		{
			response.send(Pistache::Http::Code::Not_Implemented, "Invalid value!");
			return;
		}

		// Send JSON response.
		auto result = converter<weight_metrics, distance_metrics, temperature_metrics>::instance()->process(from, to, from_value);
		if (!result)
		{
			response.send(Pistache::Http::Code::Not_Implemented, "Unknown conversion type!");
			return;
		}
		
		std::string json_response = "{\"result\":\"" + std::to_string(result.value()) + "\"}";
		response.send(Pistache::Http::Code::Ok, json_response, MIME(Text, Plain));		
    }
};

int main()
{
    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(9080));
    auto opts = Pistache::Http::Endpoint::options()
        .threads(1);

    Http::Endpoint server(addr);
    server.init(opts);
    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    server.shutdown();
}
