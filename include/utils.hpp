#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace goquant
{

    namespace utils
    {

        // Time utilities
        inline std::string getCurrentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            auto now_s = std::chrono::duration_cast<std::chrono::seconds>(now_ms);
            auto ms = now_ms - std::chrono::duration_cast<std::chrono::milliseconds>(now_s);

            time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm local_tm = *std::localtime(&tt);

            std::stringstream ss;
            ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return ss.str();
        }

        // Cryptographic utilities
        inline std::string hmacSHA256(const std::string &key, const std::string &data)
        {
            unsigned char *digest = HMAC(EVP_sha256(),
                                         key.c_str(), key.length(),
                                         (unsigned char *)data.c_str(), data.length(),
                                         nullptr, nullptr);
            if (!digest)
                return "";

            std::stringstream ss;
            for (int i = 0; i < 32; i++)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
            }
            return ss.str();
        }

        // String utilities
        inline std::string toLower(std::string str)
        {
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return str;
        }

        inline std::string toUpper(std::string str)
        {
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);
            return str;
        }

        // JSON utilities
        inline std::string jsonToString(const nlohmann::json &j)
        {
            return j.dump();
        }

        inline nlohmann::json stringToJson(const std::string &s)
        {
            return nlohmann::json::parse(s);
        }

    } // namespace utils

} // namespace goquant