#include <curl/curl.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>

#include "util/http.hpp"

namespace http 
{
    // Extracts the filename from a Content-Disposition header line
    // Searches for the "filename=" parameter and extracts the filename string
    // Returns an empty string if no filename can be extracted
    static std::string extractFilenameFromContentDisposition(const std::string &headerLine)
    {
        auto pos = headerLine.find("filename=");
        if (pos != std::string::npos)
        {
            pos += 9; // Advance position to start of filename (after "filename=")

            // If the filename is quoted, extract the quoted string
            if (pos < headerLine.size() && headerLine[pos] == '"')
            {
                auto endPos = headerLine.find('"', pos + 1);
                if (endPos != std::string::npos)
                {
                    return headerLine.substr(pos + 1, endPos - (pos + 1));
                }
            }
            else
            {
                // If not quoted, extract until the first delimiter (space, semicolon, CR or LF)
                size_t endPos = headerLine.find_first_of(" ;\r\n", pos);
                return headerLine.substr(pos, endPos - pos);
            }
        }

        return std::string(); // No valid filename found in the header line
    }

    // Callback function for processing HTTP headers received by libcurl; invoked for each header line
    // If the header contains a Content-Disposition field with a filename, it is extracted
    static size_t headerCallback(char *buffer, size_t size, size_t nmemb, void *userData)
    {
        size_t length = size * nmemb;

        std::string header(buffer, length);

        // Check if the header line contains the Content-Disposition field
        if (header.find("Content-Disposition:") != std::string::npos)
        {
            std::string fname = extractFilenameFromContentDisposition(header);
            if (!fname.empty())
            {
                // Store the extracted filename at the address provided by the user data
                std::string *resolvedName = static_cast<std::string *>(userData);
                *resolvedName = fname;
            }
        }

        return length; // Return the number of bytes processed
    }

    // Derives a filename from the provided URL
    // The function extracts the substring following the last '/' in the URL
    // If the URL does not appear to contain a valid filename, a default filename is returned
    static std::string deriveFilenameFromUrl(const std::string &url)
    {
        auto pos = url.find_last_of('/');
        if (pos == std::string::npos || pos == url.size() - 1)
        {
            // Either no '/' found or the URL ends with '/', so return the default filename
            return DEFAULT_FILENAME;
        }

        std::string fname = url.substr(pos + 1);

        auto dotPos = fname.find_last_of('.');
        if (dotPos == std::string::npos)
        {
            // No '.' found; return the default filename
            return DEFAULT_FILENAME;
        }

        size_t extLength = fname.size() - dotPos - 1;
        if (extLength < 1 || extLength > 4)
        {
            // Extension is not valid (too short or too long); return the default filename
            return DEFAULT_FILENAME;
        }

        return fname;
    }

    // Resolves the filename from the server based on HTTP headers or the URL
    // Performs an HTTP HEAD request to retrieve header information
    // If a Content-Disposition header is present and includes a filename, that name is used
    // Else, the filename is derived from the effective URL after following redirects
    std::string resolveFilenameFromServer(DownloadTask &task)
    {
        const std::string &url = task.getUrl();
        std::string resolvedName = DEFAULT_FILENAME;

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            // Unable to initialise CURL; record the FAILED and return default filename
            task.setErrorCode(CURLE_FAILED_INIT);
            return resolvedName;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);         // HEAD request
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow HTTP redirects
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resolvedName);

        CURLcode res = curl_easy_perform(curl);

        // Retrieve the HTTP response status code
        int httpStatus;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
        task.setHttpStatus(httpStatus);

        // Treat HTTP status codes 400 and above as errors
        if (res == CURLE_OK && httpStatus >= 400)
        {
            res = CURLE_HTTP_RETURNED_ERROR;
        }

        if (res == CURLE_OK)
        {
            if (resolvedName == DEFAULT_FILENAME)
            {
                // No filename was extracted from the headers, derive it from the effective URL
                char *effectiveUrl = nullptr;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
                if (effectiveUrl)
                {
                    resolvedName = deriveFilenameFromUrl(std::string(effectiveUrl));
                }
            }
        }
        else
        {
            // An FAILED occurred; record the FAILED code in the download task
            task.setErrorCode(res);
        }

        curl_easy_cleanup(curl);

        return resolvedName;
    }
}