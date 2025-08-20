#include "shared.hpp"

#if COMPILE_CURL == 1
struct WebhookData
{
    std::string url;
    std::string message;
};

struct HttpRequestData
{
    std::string url;
    std::string method;
    std::string data;
    std::map<std::string, std::string> headers;
    std::string response_body;
    long response_code;
    int callback_function;
    bool save_response;
    unsigned int levelId;
};

// Callback function to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    std::string *response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), realsize);
    return realsize;
}

void async_http_request(std::shared_ptr<HttpRequestData> data)
{
    CURL *curl;
    CURLcode responseCode;
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl)
    {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, data->url.c_str());
        
        // Set method and data
        if (data->method == "POST")
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!data->data.empty())
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data->data.c_str());
        }
        else if (data->method == "PUT")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!data->data.empty())
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data->data.c_str());
        }
        else if (data->method == "DELETE")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        // GET is the default
        
        // Set custom headers
        for (const auto& header : data->headers)
        {
            std::string header_string = header.first + ": " + header.second;
            headers = curl_slist_append(headers, header_string.c_str());
        }
        if (headers)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Set up response capture if needed
        if (data->save_response)
        {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data->response_body);
        }
        
        // Set timeout (30 seconds)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        
        // Perform the request
        responseCode = curl_easy_perform(curl);
        
        // Get response code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &data->response_code);
        
        if(responseCode != CURLE_OK)
        {
            Com_Printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(responseCode));
            data->response_code = -1; // Indicate curl error
        }

        curl_easy_cleanup(curl);
        if (headers)
            curl_slist_free_all(headers);
    }
    else
    {
        Com_Printf("curl_easy_init() failed\n");
        data->response_code = -1;
    }
    
    curl_global_cleanup();
}

void async_webhook_message(std::shared_ptr<WebhookData> data)
{
    CURL *curl;
    CURLcode responseCode;
    struct curl_slist *headers = NULL;
    std::string payload = "{\"content\":\"" + data->message + "\"}";

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl)
    {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, data->url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

        responseCode = curl_easy_perform(curl);
        if(responseCode != CURLE_OK)
            Com_Printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(responseCode));

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    else
        Com_Printf("curl_easy_init() failed\n");
    
    curl_global_cleanup();
}

void gsc_curl_webhookmessage()
{
    char *url;
    char *message;

    if (!stackGetParams("ss", &url, &message))
    {
        stackError("gsc_curl_webhookmessage() one or more arguments are undefined or have a wrong type");
        stackPushUndefined();
        return;
    }

    std::shared_ptr<WebhookData> data = std::make_shared<WebhookData>();
    data->url = url;
    data->message = message;

    std::thread(async_webhook_message, data).detach();

    stackPushBool(qtrue);  // Return true to indicate the async operation has started
}

void gsc_curl_request()
{
    char *url;
    char *method;
    char *data_param = nullptr;
    
    int argc = Scr_GetNumParam();
    
    if (argc < 2 || argc > 3)
    {
        stackError("gsc_curl_request() requires 2-3 arguments: url, method, [data]");
        stackPushUndefined();
        return;
    }
    
    if (argc == 2)
    {
        if (!stackGetParams("ss", &url, &method))
        {
            stackError("gsc_curl_request() arguments must be strings");
            stackPushUndefined();
            return;
        }
    }
    else
    {
        if (!stackGetParams("sss", &url, &method, &data_param))
        {
            stackError("gsc_curl_request() arguments must be strings");
            stackPushUndefined();
            return;
        }
    }

    std::shared_ptr<HttpRequestData> data = std::make_shared<HttpRequestData>();
    data->url = url;
    data->method = method;
    if (data_param)
        data->data = data_param;
    
    // Set default JSON content type for POST/PUT requests
    if (data->method == "POST" || data->method == "PUT")
    {
        data->headers["Content-Type"] = "application/json";
    }
    
    data->save_response = true;
    data->levelId = scrVarPub.levelId;

    std::thread(async_http_request, data).detach();

    stackPushBool(qtrue);  // Return true to indicate the async operation has started
}
#endif