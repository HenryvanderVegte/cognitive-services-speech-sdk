//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// uspimpl.c: implementation of the USP library.
//

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif


#include <vector>
#include <set>

#include <assert.h>
#include <inttypes.h>

#include "azure_c_shared_utility_xlogging_wrapper.h"
#include "azure_c_shared_utility_httpheaders_wrapper.h"
#include "azure_c_shared_utility_platform_wrapper.h"
#include "azure_c_shared_utility_urlencode_wrapper.h"

#include "uspcommon.h"
#include "uspinternal.h"

#include "transport.h"
#include "dnscache.h"
#include "metrics.h"

#include "exception.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include "string_utils.h"
#include "guid_utils.h"

#include "json.h"

using namespace std;

uint64_t telemetry_gettime()
{
    auto now = chrono::high_resolution_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace USP {

using namespace std;
using namespace Microsoft::CognitiveServices::Speech::Impl;

template <class T>
static void throw_if_null(const T* ptr, const string& name)
{
    if (ptr == NULL)
    {
        ThrowInvalidArgumentException("The argument '" + name +"' is null."); \
    }
}

inline bool contains(const string& content, const string& name)
{
    return (content.find(name) != string::npos) ? true : false;
}

// Todo(1126805) url builder + auth interfaces

const std::string g_recoModeStrings[] = { "interactive", "conversation", "dictation" };
const std::string g_outFormatStrings[] = { "simple", "detailed" };


// TODO: remove this as soon as transport.c is re-written in cpp.
extern "C" {
    const char* g_keywordContentType = headers::contentType;
}


// This is called from telemetry_flush, invoked on a worker thread in turn-end.
void Connection::Impl::OnTelemetryData(const uint8_t* buffer, size_t bytesToWrite, void *context, const char *requestId)
{
    Connection::Impl *connection = (Connection::Impl*)context;
    TransportWriteTelemetry(connection->m_transport.get(), buffer, bytesToWrite, requestId);
}


Connection::Impl::Impl(const Client& config)
    : m_config(config),
    m_connected(false),
    m_haveWork(false),
    m_audioOffset(0),
    m_creationTime(telemetry_gettime())
{
    static once_flag initOnce;

    call_once(initOnce, [] {
        if (platform_init() != 0) {
            ThrowRuntimeError("Failed to initialize platform (azure-c-shared)");
        }
    });

    Validate();
}

uint64_t Connection::Impl::getTimestamp()
{
    return telemetry_gettime() - m_creationTime;
}

void Connection::Impl::Invoke(std::function<void()> callback)
{
    if (!m_connected)
    {
        return;
    }
    m_mutex.unlock();
    callback();
    m_mutex.lock();
}

void Connection::Impl::WorkThread(weak_ptr<Connection::Impl> ptr)
{
    try
    {
        {
            auto connection = ptr.lock();
            if (connection == nullptr)
            {
                return;
            }
            connection->SignalConnected();
        }

        while (true)
        {
            auto connection = ptr.lock();
            if (connection == nullptr)
            {
                // connection is destroyed, our work here is done.
                LogInfo("%s connection destryoed.", __FUNCTION__);
                break;
            }

            unique_lock<recursive_mutex> lock(connection->m_mutex);
            if (!connection->m_connected)
            {
                return;
            }

            auto callbacks = connection->m_config.m_callbacks;

            try
            {
                TransportDoWork(connection->m_transport.get());
            }
            catch (const exception& e)
            {
                connection->Invoke([&] { callbacks->OnError(false, ErrorCode::RuntimeError, e.what()); });
            }
            catch (...)
            {
                connection->Invoke([&] { callbacks->OnError(false, ErrorCode::RuntimeError, "Unhandled exception in the USP layer."); });
            }

            connection->m_cv.wait_for(lock, chrono::milliseconds(200), [&] {return connection->m_haveWork || !connection->m_connected; });
            connection->m_haveWork = false;
        }
    }
    catch (const std::exception& ex) {
        (void)ex; // release builds
        LogError("%s Unexpected Exception %s. Thread terminated", __FUNCTION__, ex.what());
    }
    catch (...) {
        LogError("%s Unexpected Exception. Thread terminated", __FUNCTION__);
    }

    LogInfo("%s Thread ending normally.", __FUNCTION__);
}

void Connection::Impl::SignalWork()
{
    m_haveWork = true;
    m_cv.notify_one();
}

void Connection::Impl::SignalConnected()
{
    unique_lock<recursive_mutex> lock(m_mutex);
    m_connected = true;
    m_cv.notify_one();
}

void Connection::Impl::Shutdown()
{
    {
        unique_lock<recursive_mutex> lock(m_mutex);
        m_config.m_callbacks = nullptr;

        // This will force the active thread to exit at some point,
        // we do not wait on the thread in order not to block the calling side.
        m_connected = false;
        SignalWork();
    }

    // The thread has its own ref counted copy of callbacks.
    m_worker.detach();
}

void Connection::Impl::Validate()
{
    if (m_config.m_authData.empty())
    {
        ThrowInvalidArgumentException("No valid authentication mechanism was specified.");
    }
}

string Connection::Impl::EncodeParameterString(const string& parameter) const
{
    STRING_HANDLE encodedHandle = URL_EncodeString(parameter.c_str());
    string encodedStr(STRING_c_str(encodedHandle));
    STRING_delete(encodedHandle);
    return encodedStr;
}

string Connection::Impl::ConstructConnectionUrl() const
{
    auto recoMode = static_cast<underlying_type_t<RecognitionMode>>(m_config.m_recoMode);
    ostringstream oss;
    bool customEndpoint = false;

    // Using customized endpoint if it is defined.
    if (!m_config.m_customEndpointUrl.empty())
    {
        oss << m_config.m_customEndpointUrl;
        customEndpoint = true;
    }
    else
    {
        oss << endpoint::protocol;
        switch (m_config.m_endpoint)
        {
        case EndpointType::Speech:
            oss << m_config.m_region
                << endpoint::unifiedspeech::hostnameSuffix
                << endpoint::unifiedspeech::pathPrefix
                << g_recoModeStrings[recoMode]
                << endpoint::unifiedspeech::pathSuffix;
            break;
        case EndpointType::Translation:
            oss << m_config.m_region
                << endpoint::translation::hostnameSuffix
                << endpoint::translation::path;
            break;
        case EndpointType::Intent:
            oss << endpoint::luis::hostname
                << endpoint::luis::pathPrefix1
                << m_config.m_intentRegion
                << endpoint::luis::pathPrefix2
                << g_recoModeStrings[(int)RecognitionMode::Interactive]
                << endpoint::luis::pathSuffix;
            break;
        case EndpointType::CDSDK:
            oss << endpoint::CDSDK::url;
            break;
        default:
            ThrowInvalidArgumentException("Unknown endpoint type.");
        }
    }

    // The first query parameter.
    auto format = static_cast<underlying_type_t<OutputFormat>>(m_config.m_outputFormat);
    if (!customEndpoint || !contains(oss.str(), endpoint::unifiedspeech::outputFormatQueryParam))
    {
        auto delim = (customEndpoint && (oss.str().find('?') != string::npos)) ? '&' : '?';
        oss << delim << endpoint::unifiedspeech::outputFormatQueryParam << g_outFormatStrings[format];
    }

    // Todo: use libcurl or another library to encode the url as whole, instead of each parameter.
    switch (m_config.m_endpoint)
    {
    case EndpointType::Speech:
        if (!m_config.m_modelId.empty())
        {
            if (!customEndpoint || !contains(oss.str(), endpoint::unifiedspeech::deploymentIdQueryParam))
            {
                oss << '&' << endpoint::unifiedspeech::deploymentIdQueryParam << m_config.m_modelId;
            }
        }
        else if (!m_config.m_language.empty())
        {
            if (!customEndpoint || !contains(oss.str(), endpoint::unifiedspeech::langQueryParam))
            {
                oss << '&' << endpoint::unifiedspeech::langQueryParam << m_config.m_language;
            }
        }
        break;
    case EndpointType::Intent:
        if (!m_config.m_language.empty())
        {
            if (!customEndpoint || !contains(oss.str(), endpoint::unifiedspeech::langQueryParam))
            {
                oss << '&' << endpoint::unifiedspeech::langQueryParam << m_config.m_language;
            }
        }
        break;
    case EndpointType::Translation:
        if (!customEndpoint || !contains(oss.str(), endpoint::translation::from))
        {
            oss << '&' << endpoint::translation::from << EncodeParameterString(m_config.m_translationSourceLanguage);
        }
        if (!customEndpoint || !contains(oss.str(), endpoint::translation::to))
        {
            size_t start = 0;
            auto delim = ',';
            size_t end = m_config.m_translationTargetLanguages.find_first_of(delim);
            while (end != string::npos)
            {
                oss << '&' << endpoint::translation::to << EncodeParameterString(m_config.m_translationTargetLanguages.substr(start, end - start));
                start = end + 1;
                end = m_config.m_translationTargetLanguages.find_first_of(delim, start);
            }
            oss << '&' << endpoint::translation::to << EncodeParameterString(m_config.m_translationTargetLanguages.substr(start, end));
        }

        if (!m_config.m_translationVoice.empty())
        {
            if (!customEndpoint || !contains(oss.str(), endpoint::translation::voice))
            {
                oss << '&' << endpoint::translation::features << endpoint::translation::requireVoice;
                oss << '&' << endpoint::translation::voice << EncodeParameterString(m_config.m_translationVoice);
            }
        }
        break;
    case EndpointType::CDSDK:
        // no query parameter needed.
        break;
    }

    return oss.str();
}

void Connection::Impl::Connect()
{
    if (m_transport != nullptr || m_connected)
    {
        ThrowLogicError("USP connection already created.");
    }

    using HeadersPtr = deleted_unique_ptr<remove_pointer<HTTP_HEADERS_HANDLE>::type>;

    HeadersPtr connectionHeaders(HTTPHeaders_Alloc(), HTTPHeaders_Free);

    if (connectionHeaders == nullptr)
    {
        ThrowRuntimeError("Failed to create connection headers.");
    }

    auto headersPtr = connectionHeaders.get();

    if (m_config.m_endpoint == EndpointType::CDSDK)
    {
        // TODO: MSFT: 1135317 Allow for configurable audio format
        HTTPHeaders_ReplaceHeaderNameValuePair(headersPtr, headers::audioResponseFormat, "riff-16khz-16bit-mono-pcm");
        HTTPHeaders_ReplaceHeaderNameValuePair(headersPtr, headers::userAgent, g_userAgent);
    }

    assert(!m_config.m_authData.empty());

    switch (m_config.m_authType)
    {
    case AuthenticationType::SubscriptionKey:
        if (HTTPHeaders_ReplaceHeaderNameValuePair(headersPtr, headers::ocpApimSubscriptionKey, m_config.m_authData.c_str()) != 0)
        {
            ThrowRuntimeError("Failed to set authentication using subscription key.");
        }
        break;

    case AuthenticationType::AuthorizationToken:
        {
            ostringstream oss;
            oss << "Bearer " << m_config.m_authData;
            auto token = oss.str();
            if (HTTPHeaders_ReplaceHeaderNameValuePair(headersPtr, headers::authorization, token.c_str()) != 0)
            {
                ThrowRuntimeError("Failed to set authentication using authorization token.");
            }
        }
        break;

    // TODO(1126805): url builder + auth interfaces
    case AuthenticationType::SearchDelegationRPSToken:
        if (HTTPHeaders_ReplaceHeaderNameValuePair(headersPtr, headers::searchDelegationRPSToken, m_config.m_authData.c_str()) != 0)
        {
            ThrowRuntimeError("Failed to set authentication using Search-DelegationRPSToken.");
        }
        break;

    default:
        ThrowRuntimeError("Unsupported authentication type");
    }

    auto connectionUrl = ConstructConnectionUrl();
    LogInfo("connectionUrl=%s", connectionUrl.c_str());

    m_telemetry = TelemetryPtr(telemetry_create(Connection::Impl::OnTelemetryData, this), telemetry_destroy);
    if (m_telemetry == nullptr)
    {
        ThrowRuntimeError("Failed to create telemetry instance.");
    }

    std::string connectionId = PAL::ToString(m_config.m_connectionId);

    // Log the device uuid
    metrics_device_startup(m_telemetry.get(), connectionId.c_str(), PAL::DeviceUuid().c_str());

    m_transport = TransportPtr(TransportRequestCreate(connectionUrl.c_str(), this, m_telemetry.get(), headersPtr, connectionId.c_str()), TransportRequestDestroy);
    if (m_transport == nullptr)
    {
        ThrowRuntimeError("Failed to create transport request.");
    }

#ifdef __linux__
    m_dnsCache = DnsCachePtr(DnsCacheCreate(), DnsCacheDestroy);
    if (!m_dnsCache)
    {
        ThrowRuntimeError("Failed to create DNS cache.");
    }
#else
    m_dnsCache = nullptr;
#endif

    TransportSetDnsCache(m_transport.get(), m_dnsCache.get());
    TransportSetCallbacks(m_transport.get(), OnTransportError, OnTransportData);

    m_worker = thread(&Connection::Impl::WorkThread, shared_from_this());
    unique_lock<recursive_mutex> lock(m_mutex);
    m_cv.wait(lock, [&] {return m_connected; });
}

string Connection::Impl::CreateRequestId()
{
    auto requestId = PAL::ToString(PAL::CreateGuidWithoutDashes());

    LogInfo("RequestId: '%s'", requestId.c_str());
    metrics_transport_requestid(m_telemetry.get(), requestId.c_str());

    m_activeRequestIds.insert(requestId);

    return requestId;
}

void Connection::Impl::QueueMessage(const string& path, const uint8_t *data, size_t size, MessageType messageType)
{
    unique_lock<recursive_mutex> lock(m_mutex);

    throw_if_null(data, "data");

    if (path.empty())
    {
        ThrowInvalidArgumentException("The path is null or empty.");
    }

    if (m_connected)
    {
        // If the service receives multiple context messages for a single turn,
        // the service will close the WebSocket connection with an error.
        if (messageType == MessageType::Context && !m_speechRequestId.empty())
        {
            ThrowLogicError("Error trying to send a context message while in the middle of a speech turn.");
        }

        // The config message does not require a X-RequestId header, because this message is not associated with a particular request.
        string requestId;
        if (messageType != MessageType::Config)
        {
           requestId = CreateRequestId();
           m_speechRequestId = (messageType == MessageType::Context) ? requestId : m_speechRequestId;
        }

        (void)TransportMessageWrite(m_transport.get(), path.c_str(), data, size, requestId.c_str());
    }

    SignalWork();
}

void Connection::Impl::QueueAudioSegment(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        QueueAudioEnd();
        return;
    }

    unique_lock<recursive_mutex> lock(m_mutex);

    LogInfo("TS:%" PRIu64 ", Write %" PRIu32 " bytes audio data.", getTimestamp(), size);

    throw_if_null(data, "data");

    if (!m_connected)
    {
        return;
    }

    metrics_audiostream_data(size);

    int ret = 0;

    if (m_audioOffset == 0)
    {
        // The service uses the first audio message that contains a unique request identifier to signal the start of a new request/response cycle or turn.
        // After receiving an audio message with a new request identifier, the service discards any queued or unsent messages
        // that are associated with any previous turn.
        m_speechRequestId = m_speechRequestId.empty() ? CreateRequestId() : m_speechRequestId;
        metrics_audiostream_init();
        metrics_audio_start(m_telemetry.get(), m_speechRequestId.c_str());

        ret = TransportStreamPrepare(m_transport.get(), "/audio");
        if (ret != 0)
        {
            ThrowRuntimeError("TransportStreamPrepare failed. error=" + to_string(ret));
        }
    }

    ret = TransportStreamWrite(m_transport.get(), data, size, m_speechRequestId.c_str());
    if (ret != 0)
    {
        ThrowRuntimeError("TransportStreamWrite failed. error=" + to_string(ret));
    }

    m_audioOffset += size;
    SignalWork();
}

void Connection::Impl::QueueAudioEnd()
{
    unique_lock<recursive_mutex> lock(m_mutex);
    LogInfo("TS:%" PRIu64 ", Flush audio buffer.", getTimestamp());

    if (!m_connected || m_audioOffset == 0)
    {
        return;
    }

    auto ret = TransportStreamFlush(m_transport.get(), m_speechRequestId.c_str());

    m_audioOffset = 0;
    metrics_audiostream_flush();
    metrics_audio_end(m_telemetry.get(), m_speechRequestId.c_str());

    if (ret != 0)
    {
        ThrowRuntimeError("Returns failure, reason: TransportStreamFlush returned " + to_string(ret));
    }
    SignalWork();
}

// Callback for transport errors
void Connection::Impl::OnTransportError(TransportHandle transportHandle, TransportErrorInfo* errorInfo, void* context)
{
    (void)transportHandle;
    throw_if_null(context, "context");

    Connection::Impl *connection = static_cast<Connection::Impl*>(context);

    auto errorStr = (errorInfo->errorString != nullptr) ? errorInfo->errorString : "";
    LogInfo("TS:%" PRIu64 ", TransportError: connection:0x%x, reason=%d, code=%d [0x%08x], string=%s",
        connection->getTimestamp(), connection, errorInfo->reason, errorInfo->errorCode, errorInfo->errorCode, errorStr);

    auto callbacks = connection->m_config.m_callbacks;

    switch (errorInfo->reason)
    {
    case TRANSPORT_ERROR_REMOTE_CLOSED:
        connection->Invoke([&] {
            callbacks->OnError(true, ErrorCode::ConnectionError, "Connection was closed by the remote host. Error code: " + to_string(errorInfo->errorCode) + ". Error details: " + errorStr);
        });
        break;

    case TRANSPORT_ERROR_CONNECTION_FAILURE:
        connection->Invoke([&] {
            callbacks->OnError(true, ErrorCode::ConnectionError,
                std::string("Connection failed (no connection to the remote host). Internal error: ") +
                std::to_string(errorInfo->errorCode) + ". Error details: " + errorStr +
                std::string(". Please check network connection, firewall setting, and the region name used to create speech factory.")); });
        break;

    case TRANSPORT_ERROR_WEBSOCKET_UPGRADE:
        switch (errorInfo->errorCode)
        {
        case HTTP_BADREQUEST:
            connection->Invoke([&] { callbacks->OnError(true, ErrorCode::BadRequest,
                "WebSocket Upgrade failed with a bad request (400). Please check the language name and endpoint id (if used) are correctly associated with the provided subscription key."); });
            break;
        case HTTP_UNAUTHORIZED:
            connection->Invoke([&] { callbacks->OnError(true, ErrorCode::AuthenticationError,
                "WebSocket Upgrade failed with an authentication error (401). Please check for correct subscription key (or authorization token) and region name."); });
            break;
        case HTTP_FORBIDDEN:
            connection->Invoke([&] { callbacks->OnError(true, ErrorCode::AuthenticationError,
                "WebSocket Upgrade failed with an authentication error (403). Please check for correct subscription key (or authorization token) and region name."); });
            break;
        case HTTP_TOO_MANY_REQUESTS:
            connection->Invoke([&] { callbacks->OnError(true, ErrorCode::TooManyRequests,
                "WebSocket Upgrade failed with too many requests error (429). Please check for correct subscription key (or authorization token) and region name."); });
            break;
        default:
            connection->Invoke([&] { callbacks->OnError(true, ErrorCode::ConnectionError,
                "WebSocket Upgrade failed with HTTP status code: " + std::to_string(errorInfo->errorCode)); });
            break;
        }
        break;

    case TRANSPORT_ERROR_WEBSOCKET_SEND_FRAME:
        connection->Invoke([&] {
            callbacks->OnError(true, ErrorCode::ConnectionError,
                std::string("Failure while sending a frame over the WebSocket connection. Internal error: ") +
                std::to_string(errorInfo->errorCode) + ". Error details: " + errorStr);
        });
        break;

    case TRANSPORT_ERROR_WEBSOCKET_ERROR:
        connection->Invoke([&] { callbacks->OnError(true, ErrorCode::ConnectionError,
            std::string("WebSocket operation failed. Internal error: ") +
            std::to_string(errorInfo->errorCode) + ". Error details: " + errorStr);
        });
        break;

    case TRANSPORT_ERROR_DNS_FAILURE:
        connection->Invoke([&] { callbacks->OnError(true, ErrorCode::ConnectionError,
            std::string("DNS connection failed (the remote host did not respond). Internal error: ") + std::to_string(errorInfo->errorCode));
        });
        break;

    default:
    case TRANSPORT_ERROR_UNKNOWN:
        connection->Invoke([&] { callbacks->OnError(true, ErrorCode::ConnectionError, "Unknown transport error."); });
        break;
    }

    connection->m_connected = false;
}

static RecognitionStatus ToRecognitionStatus(const string& str)
{
    if (0 == str.compare("Success")) return  RecognitionStatus::Success;
    if (0 == str.compare("NoMatch")) return  RecognitionStatus::NoMatch;
    if (0 == str.compare("InitialSilenceTimeout")) return  RecognitionStatus::InitialSilenceTimeout;
    if (0 == str.compare("BabbleTimeout")) return RecognitionStatus::InitialBabbleTimeout;
    if (0 == str.compare("Error")) return RecognitionStatus::Error;
    if (0 == str.compare("EndOfDictation")) return RecognitionStatus::EndOfDictation;
    if (0 == str.compare("TooManyRequests")) return RecognitionStatus::TooManyRequests;
    if (0 == str.compare("BadRequest")) return RecognitionStatus::BadRequest;
    if (0 == str.compare("Forbidden")) return RecognitionStatus::Forbidden;
    if (0 == str.compare("ServiceUnavailable")) return RecognitionStatus::ServiceUnavailable;

    PROTOCOL_VIOLATION("Unknown RecognitionStatus: %s", str.c_str());
    return RecognitionStatus::InvalidMessage;
}

static TranslationStatus ToTranslationStatus(const string& str)
{
    if (0 == str.compare("Success")) return  TranslationStatus::Success;
    if (0 == str.compare("Error")) return  TranslationStatus::Error;

    PROTOCOL_VIOLATION("Unknown TranslationStatus: %s", str.c_str());
    return TranslationStatus::InvalidMessage;
}

static SynthesisStatus ToSynthesisStatus(const string& str)
{
    if (0 == str.compare("Success")) return  SynthesisStatus::Success;
    if (0 == str.compare("Error")) return  SynthesisStatus::Error;

    PROTOCOL_VIOLATION("Unknown SynthesisStatus: %s", str.c_str());
    return SynthesisStatus::InvalidMessage;
}

static SpeechHypothesisMsg RetrieveSpeechResult(const nlohmann::json& json)
{
    auto offset = json.at(json_properties::offset).get<OffsetType>();
    auto duration = json.at(json_properties::duration).get<DurationType>();
    auto textObj = json.find(json_properties::text);
    string text;
    if (textObj != json.end())
    {
        text = json.at(json_properties::text).get<string>();
    }
    return SpeechHypothesisMsg(PAL::ToWString(json.dump()), offset, duration, PAL::ToWString(text));
}

static TranslationResult RetrieveTranslationResult(const nlohmann::json& json, bool expectStatus)
{
    auto translation = json[json_properties::translation];

    TranslationResult result;
    if (expectStatus)
    {
        auto status = translation.find(json_properties::translationStatus);
        if (status != translation.end())
        {
            result.translationStatus = ToTranslationStatus(status->get<string>());
        }
        else
        {
            PROTOCOL_VIOLATION("No TranslationStatus is provided. Json: %s", translation.dump().c_str());
            result.translationStatus = TranslationStatus::InvalidMessage;
            result.failureReason = L"Status is missing in the protocol message. Response text:" + PAL::ToWString(json.dump());
        }

        auto failure = translation.find(json_properties::translationFailureReason);
        if (failure != translation.end())
        {
            result.failureReason += PAL::ToWString(failure->get<string>());
        }
    }

    if (expectStatus && result.translationStatus != TranslationStatus::Success)
    {
        return result;
    }
    else
    {
        auto translations = translation.at(json_properties::translations);
        for (const auto& object : translations)
        {
            auto lang = object.at(json_properties::lang).get<string>();
            auto txt = object.at(json_properties::text).get<string>();
            if (lang.empty() && txt.empty())
            {
                PROTOCOL_VIOLATION("emtpy language and text field in translations text. lang=%s, text=%s.", lang.c_str(), txt.c_str());
                continue;
            }

            result.translations[PAL::ToWString(lang)] = PAL::ToWString(txt);
        }

        if (!result.translations.size())
        {
            PROTOCOL_VIOLATION("No Translations text block in the message. Response text:", json.dump().c_str());
        }
        return result;
    }
}

// Callback for data available on tranport
void Connection::Impl::OnTransportData(TransportHandle transportHandle, HTTP_HEADERS_HANDLE responseHeader, const unsigned char* buffer, size_t size, unsigned int errorCode, void* context)
{
    (void)transportHandle;
    throw_if_null(context, "context");

    Connection::Impl *connection = static_cast<Connection::Impl*>(context);

    if (errorCode != 0)
    {
        LogError("Response error %d.", errorCode);
        // TODO: Lower layers need appropriate signals
        return;
    }
    else if (responseHeader == NULL)
    {
        LogError("ResponseHeader is NULL.");
        return;
    }

    string requestId = HTTPHeaders_FindHeaderValue(responseHeader, headers::requestId);
    if (requestId.empty() || !connection->m_activeRequestIds.count(requestId))
    {
        PROTOCOL_VIOLATION("Unexpected request id '%s', Path: %s", requestId.c_str(),
                           HTTPHeaders_FindHeaderValue(responseHeader, KEYWORD_PATH));
        metrics_unexpected_requestid(requestId.c_str());
        return;
    }

    auto path = HTTPHeaders_FindHeaderValue(responseHeader, KEYWORD_PATH);
    if (path == NULL)
    {
        PROTOCOL_VIOLATION("response missing '%s' header", KEYWORD_PATH);
        return;
    }

    const char* contentType = NULL;
    if (size != 0)
    {
        contentType = HTTPHeaders_FindHeaderValue(responseHeader, headers::contentType);
        if (contentType == NULL)
        {
            PROTOCOL_VIOLATION("response '%s' contains body with no content-type", path);
            return;
        }
    }

    metrics_received_message(connection->m_telemetry.get(), requestId.c_str(), path);

    LogInfo("TS:%" PRIu64 " Response Message: path: %s, content type: %s, size: %zu.", connection->getTimestamp(), path, contentType, size);

    string pathStr(path);
    auto callbacks = connection->m_config.m_callbacks;

    // TODO: pass the frame type (binary/text) so that we can check frame type before calling json::parse.
    if (pathStr == path::translationSynthesis)
    {
        TranslationSynthesisMsg msg;
        msg.audioBuffer = (uint8_t *)buffer;
        msg.audioLength = size;
        connection->Invoke([&] { callbacks->OnTranslationSynthesis(msg); });
        return;
    }

    auto json = (size > 0) ? nlohmann::json::parse(buffer, buffer + size) : nlohmann::json();
    if (pathStr == path::speechStartDetected || path == path::speechEndDetected)
    {
        auto offsetObj = json[json_properties::offset];
        // For whatever reason, offset is sometimes missing on the end detected message.
        auto offset = offsetObj.is_null() ? 0 : offsetObj.get<OffsetType>();

        if (path == path::speechStartDetected)
        {
            connection->Invoke([&] { callbacks->OnSpeechStartDetected({ PAL::ToWString(json.dump()), offset }); });
        }
        else
        {
            connection->Invoke([&] { callbacks->OnSpeechEndDetected({ PAL::ToWString(json.dump()), offset }); });
        }
    }
    else if (pathStr == path::turnStart)
    {
        auto tag = json[json_properties::context][json_properties::tag].get<string>();
        connection->Invoke([&] { callbacks->OnTurnStart({ PAL::ToWString(json.dump()), tag }); });
    }
    else if (pathStr == path::turnEnd)
    {
        {
            unique_lock<recursive_mutex> lock(connection->m_mutex);
            if (requestId == connection->m_speechRequestId)
            {
                connection->m_speechRequestId.clear();
            }
            connection->m_activeRequestIds.erase(requestId);
        }

        // flush the telemetry before invoking the onTurnEnd callback.
        // TODO: 1164154
        telemetry_flush(connection->m_telemetry.get(), requestId.c_str());

        connection->Invoke([&] { callbacks->OnTurnEnd({ }); });
    }
    else if (path == path::speechHypothesis || path == path::speechFragment)
    {
        auto offset = json[json_properties::offset].get<OffsetType>();
        auto duration = json[json_properties::duration].get<DurationType>();
        auto text = json[json_properties::text].get<string>();

        if (path == path::speechHypothesis)
        {
            connection->Invoke([&] { callbacks->OnSpeechHypothesis({
                PAL::ToWString(json.dump()),
                offset,
                duration,
                PAL::ToWString(text)
                });
            });
        }
        else
        {
            connection->Invoke([&] { callbacks->OnSpeechFragment({
                PAL::ToWString(json.dump()),
                offset,
                duration,
                PAL::ToWString(text)
                });
            });
        }
    }
    else if (path == path::speechPhrase)
    {
        SpeechPhraseMsg result;
        result.json = PAL::ToWString(json.dump());
        result.offset = json[json_properties::offset].get<OffsetType>();
        result.duration = json[json_properties::duration].get<DurationType>();
        result.recognitionStatus = ToRecognitionStatus(json[json_properties::recoStatus].get<string>());

        switch (result.recognitionStatus)
        {
        case RecognitionStatus::Success:
            if (json.find(json_properties::displayText) != json.end())
            {
                // The DisplayText field will be present only if the RecognitionStatus field has the value Success.
                // and the format output is simple
                result.displayText = PAL::ToWString(json[json_properties::displayText].get<string>());
            }
            else // Detailed
            {
                auto phrases  = json.at(json_properties::nbest);

                double confidence = 0;
                for (const auto& object : phrases)
                {
                    auto currentConfidence = object.at(json_properties::confidence).get<double>();
                    // Picking up the result with the highest confidence.
                    if (currentConfidence > confidence)
                    {
                        confidence = currentConfidence;
                        result.displayText = PAL::ToWString(object.at(json_properties::display).get<string>());
                    }
                }
            }
            connection->Invoke([&] { callbacks->OnSpeechPhrase(result); });
            break;
        case RecognitionStatus::InitialSilenceTimeout:
        case RecognitionStatus::InitialBabbleTimeout:
        case RecognitionStatus::NoMatch:
        case RecognitionStatus::EndOfDictation:
            connection->Invoke([&] { callbacks->OnSpeechPhrase(result); });
            break;
        default:
            connection->InvokeRecognitionErrorCallback(result.recognitionStatus, json.dump());
            break;
        }
    }
    else if (path == path::translationHypothesis)
    {
        auto speechResult = RetrieveSpeechResult(json);
        auto translationResult = RetrieveTranslationResult(json, false);
        // TranslationStatus is always success for translation.hypothesis
        translationResult.translationStatus = TranslationStatus::Success;

        connection->Invoke([&] { callbacks->OnTranslationHypothesis({
            std::move(speechResult.json),
            speechResult.offset,
            speechResult.duration,
            std::move(speechResult.text),
            std::move(translationResult)
            });
        });
    }
    else if (path == path::translationPhrase)
    {
        auto status = ToRecognitionStatus(json.at(json_properties::recoStatus));
        auto speechResult = RetrieveSpeechResult(json);
        std::string msg;

        TranslationResult translationResult;
        switch (status)
        {
        case RecognitionStatus::Success:
            translationResult = RetrieveTranslationResult(json, true);
            break;
        case RecognitionStatus::InitialSilenceTimeout:
        case RecognitionStatus::InitialBabbleTimeout:
        case RecognitionStatus::NoMatch:
        case RecognitionStatus::EndOfDictation:
            translationResult.translationStatus = TranslationStatus::Success;
            break;
        default:
            connection->InvokeRecognitionErrorCallback(status, json.dump());
            break;
        }

        if (translationResult.translationStatus == TranslationStatus::Success)
        {
            connection->Invoke([&] { callbacks->OnTranslationPhrase({
                std::move(speechResult.json),
                speechResult.offset,
                speechResult.duration,
                std::move(speechResult.text),
                std::move(translationResult),
                status
                });
            });
        }
    }
    else if (path == path::translationSynthesisEnd)
    {
        TranslationSynthesisEndMsg synthesisEndMsg;
        std::wstring localReason;

        auto statusHandle = json.find(json_properties::synthesisStatus);
        if (statusHandle != json.end())
        {
            synthesisEndMsg.synthesisStatus = ToSynthesisStatus(statusHandle->get<string>());
            if (synthesisEndMsg.synthesisStatus == SynthesisStatus::InvalidMessage)
            {
                PROTOCOL_VIOLATION("Invalid synthesis status in synthesis.end message. Json=%s", json.dump().c_str());
                localReason = L"Invalid synthesis status in synthesis.end message.";
            }
        }
        else
        {
            PROTOCOL_VIOLATION("No synthesis status in synthesis.end message. Json=%s", json.dump().c_str());
            synthesisEndMsg.synthesisStatus = SynthesisStatus::InvalidMessage;
            localReason = L"No synthesis status in synthesis.end message.";
        }

        auto failureHandle = json.find(json_properties::translationFailureReason);
        if (failureHandle != json.end())
        {
            if (synthesisEndMsg.synthesisStatus == SynthesisStatus::Success)
            {
                PROTOCOL_VIOLATION("FailureReason should be empty if SynthesisStatus is success. Json=%s", json.dump().c_str());
            }
            synthesisEndMsg.failureReason = PAL::ToWString(failureHandle->get<string>());
        }

        synthesisEndMsg.failureReason = localReason + synthesisEndMsg.failureReason;

        if (synthesisEndMsg.synthesisStatus == SynthesisStatus::Success)
        {
            connection->Invoke([&] { callbacks->OnTranslationSynthesisEnd(synthesisEndMsg); });
        }
        else
        {
            connection->Invoke([&] { callbacks->OnError(false, ErrorCode::ServiceError, PAL::ToString(synthesisEndMsg.failureReason).c_str()); });
        }
    }
    else
    {
        connection->Invoke([&] { callbacks->OnUserMessage({
            pathStr,
            string(contentType == nullptr ? "" : contentType),
            buffer,
            size
            });
        });
    }
}

void Connection::Impl::InvokeRecognitionErrorCallback(RecognitionStatus status, const std::string& response)
{
    auto callbacks = m_config.m_callbacks;
    string msg;

    switch (status)
    {
    case RecognitionStatus::Error:
        msg = "The speech recognition service encountered an internal error and could not continue. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::ServiceError, msg); });
        break;
    case RecognitionStatus::TooManyRequests:
        msg = "The number of parallel requests exceeded the number of allowed concurrent transcriptions. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::TooManyRequests, msg); });
        break;
    case RecognitionStatus::BadRequest:
        msg = "Invalid parameter or unsupported audio format in the request. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::BadRequest, msg.c_str()); });
        break;
    case RecognitionStatus::Forbidden:
        msg = "The recognizer is using a free subscription that ran out of quota. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::Forbidden, msg.c_str()); });
        break;
    case RecognitionStatus::ServiceUnavailable:
        msg = "The service is currently unavailable. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::ServiceUnavailable, msg.c_str()); });
        break;
    case RecognitionStatus::InvalidMessage:
        msg = "Invalid response. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::ServiceError, msg.c_str()); });
        break;
    case RecognitionStatus::Success:
    case RecognitionStatus::EndOfDictation:
    case RecognitionStatus::InitialSilenceTimeout:
    case RecognitionStatus::InitialBabbleTimeout:
    case RecognitionStatus::NoMatch:
        msg = "Runtime Error: invoke error callback for non-error recognition status. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::RuntimeError, msg.c_str()); });
        break;
    default:
        msg = "Runtime Error: invalid recognition status. Response text:" + response;
        this->Invoke([&] { callbacks->OnError(false, ErrorCode::RuntimeError, msg.c_str()); });
        break;
    }
}

}}}}