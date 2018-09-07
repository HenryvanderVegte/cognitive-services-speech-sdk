//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// carbon_test_console.h: Declarations for CarbonTestConsole tool C++ class
//

#pragma once


using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Intent;
using namespace Microsoft::CognitiveServices::Speech::Translation;


class CarbonTestConsole
{
public:

    CarbonTestConsole();
    ~CarbonTestConsole();

    int Run(const std::vector<std::string>& args);

private:

    struct ConsoleArgs
    {
        bool m_fWaitForDebugger = false;

        std::string m_strRecognizerType;
        std::string m_strUseRecoEngineProperty;
        std::string m_strUseLuEngineProperty;
        bool m_fCommandSystem = false;

        std::string m_audioInput;
        bool m_useInteractiveMicrophone = false;
        bool m_useMockMicrophone = false;

        bool m_useMockKws = false;

        std::string m_strMockMicrophoneRealTimePercentage;
        int16_t m_mockMicrophoneRealTimePercentage = 100;

        std::string m_mockWavFileName;

        std::string m_strEndpointUri;
        std::string m_strCustomSpeechModelId;
        std::string m_strSubscriptionKey;
        std::string m_strRegion;
        std::string m_strIntentAppId;
        std::string m_strIntentNames; // comma-separated

        bool m_fRecognizeAsync = false;

        bool m_fContinuousRecognition = false;
        std::string m_strContinuousRecognitionSeconds;
        uint16_t m_continuousRecognitionSeconds = 0;

        std::string m_strRunSampleName;
        std::string m_strHowManyTimes;
        uint16_t m_runHowManyTimes = 1;

        bool m_fInteractivePrompt = false;
    };

    bool ParseConsoleArgs(const std::vector<std::string>& args, ConsoleArgs* pconsoleArgs);
    bool ValidateConsoleArgs(ConsoleArgs* pconsoleArgs);
    void ProcessConsoleArgs(ConsoleArgs* pconsoleArgs);

    void DisplayConsoleHeader();
    void DisplayConsoleUsage();
    void DisplayConsolePrompt();

    void ConsoleWrite(const char* psz, ...);
    void ConsoleWriteLine(const char* pszFormat, ...);
    bool ConsoleReadLine(std::string& str);

    bool GetConsoleInput(std::string& str);
    void ProcessConsoleInput(const char* psz);

    void ConsoleInput_Help();
    void ConsoleInput_HelpOn(const char* psz);
    void ConsoleInput_HelpOnGlobal();
    void ConsoleInput_HelpOnFactory();
    void ConsoleInput_HelpOnRecognizer();
    void ConsoleInput_HelpOnSpeech();
    void ConsoleInput_HelpOnIntent();
    void ConsoleInput_HelpOnSession();
    void ConsoleInput_HelpOnCommandSystem();

    void ConsoleInput_Factory(const char* psz);
    void ConsoleInput_Recognizer(const char* psz, std::shared_ptr<BaseAsyncRecognizer>& recognizer);
    void ConsoleInput_SpeechRecognizer(const char* psz, std::shared_ptr<SpeechRecognizer>& speechRecognizer);
    void ConsoleInput_IntentRecognizer(const char* psz, std::shared_ptr<IntentRecognizer>& intentRecognizer);

    void Factory_CreateSpeechRecognizer(const char* psz);
    // TODO: RobCh: Intent: Extend CarbonX Factory console method to be able to create intent recognizer
    // void Factory_CreateIntentRecognizer(const char* psz);

    template <class T>
    void Recognizer_IsEnabled(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_Enable(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_Disable(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_Recognize(std::shared_ptr<T>& recognizer);
    void Recognizer_Recognize(std::shared_ptr<SpeechRecognizer>& recognizer);
    void Recognizer_Recognize(std::shared_ptr<IntentRecognizer>& recognizer);
    void Recognizer_Recognize(std::shared_ptr<TranslationRecognizer>& recognizer);

    template <class T>
    void Recognizer_StartContinuousRecognition(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_StopContinuousRecognition(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_StartKeywordRecognition(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_StopKeywordRecognition(std::shared_ptr<T>& recognizer);

    template <class T>
    void Recognizer_Event(const char* psz, EventSignal<T>& recognizerEvent, typename::EventSignal<T>::CallbackFunction callback);

    void Recognizer_SessionStartedHandler(const SessionEventArgs& e) { ConsoleWriteLine("SessionStartedHandler: %s", e.SessionId.c_str()); };
    void Recognizer_SessionStoppedHandler(const SessionEventArgs& e) { ConsoleWriteLine("SessionStoppedHandler: %s", e.SessionId.c_str()); };
    void Recognizer_SpeechStartDetectedHandler(const RecognitionEventArgs& e) { ConsoleWriteLine("SpeechStartDetectedHandler: Session ID : %s , Offset : %llu", e.SessionId.c_str(), e.Offset); };
    void Recognizer_SpeechEndDetectedHandler(const RecognitionEventArgs& e) { ConsoleWriteLine("SpeechEndDetectedHandler: Session ID : %s, Offset : %llu", e.SessionId.c_str(), e.Offset); };

    void Recognizer_IntermediateResultHandler(const RecognitionEventArgs& e) { UNUSED(e); };
    void Recognizer_FinalResultHandler(const RecognitionEventArgs& e) { UNUSED(e); };
    void Recognizer_NoMatchHandler(const RecognitionEventArgs& e) { UNUSED(e); };
    void Recognizer_CanceledHandler(const RecognitionEventArgs& e) { UNUSED(e); };

    void SpeechRecognizer_IntermediateResultHandler(const SpeechRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("IntermediateResultHandler: %ls", ToString(e).c_str()); };
    void SpeechRecognizer_FinalResultHandler(const SpeechRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("FinalResultHandler: %ls", ToString(e).c_str()); }
    void SpeechRecognizer_NoMatchHandler(const SpeechRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("NoMatchHandler: %ls", ToString(e).c_str()); }
    void SpeechRecognizer_CanceledHandler(const SpeechRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("CanceledHandler: %ls", ToString(e).c_str()); };

    void TranslationRecognizer_IntermediateResultHandler(const TranslationTextResultEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("Translation IntermediateResultHandler: %s", ToString(e).c_str()); };
    void TranslationRecognizer_FinalResultHandler(const TranslationTextResultEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("Translation FinalResultHandler: %s", ToString(e).c_str()); }
    void TranslationRecognizer_SynthesisResultHandler(const TranslationSynthesisResultEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("Translation SynthesisResultHandler: %s", ToString(e).c_str()); }
    void TranslationRecognizer_ErrorHandler(const TranslationSynthesisResultEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("Translation ErrorHandler: %s", ToString(e).c_str()); }

    void IntentRecognizer_IntermediateResultHandler(const IntentRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("IntermediateResultHandler: %s", ToString(e).c_str()); };
    void IntentRecognizer_FinalResultHandler(const IntentRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("FinalResultHandler: %s", ToString(e).c_str()); }
    void IntentRecognizer_NoMatchHandler(const IntentRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("NoMatchHandler: %s", ToString(e).c_str()); }
    void IntentRecognizer_CanceledHandler(const IntentRecognitionEventArgs& e) { SPX_DBG_TRACE_VERBOSE("%s: %s", __FUNCTION__, ToString(e).c_str()); ConsoleWriteLine("CanceledHandler: %s", ToString(e).c_str()); };

    bool ToBool(const char* psz);

    std::string ToString(bool f);
    std::string ToString(const SpeechRecognitionEventArgs& e);
    std::string ToString(const IntentRecognitionEventArgs& e);
    std::string ToString(const TranslationTextResultEventArgs& e);
    std::string ToString(const TranslationSynthesisResultEventArgs& e);

    void ConsoleInput_Session(const char*);

    void Session_FromSpeechRecognizer();
    // TODO: RobCh: Intent: Extend CarbonX session capability to come from intent recognizer
    // void Session_FromIntentRecognizer();

    template <class T>
    void Parameters_SetString(T &parameters, const char* psz);

    template <class T>
    void Parameters_GetString(T &parameters, const char* psz);

    template <class T>
    void Parameters_SetNumber(T &parameters, const char* psz);

    template <class T>
    void Parameters_GetNumber(T &parameters, const char* psz);

    template <class T>
    void Parameters_SetBool(T &parameters, const char* psz);

    template <class T>
    void Parameters_GetBool(T &parameters, const char* psz);

    void ConsoleInput_CommandSystem(const char* psz);

    void InitGlobalParameters(ConsoleArgs* pconsoleArgs);

    bool ShouldInitCarbon() { return m_recognizer == nullptr; }
    void EnsureInitCarbon(ConsoleArgs* pconsoleArgs);

    void InitCarbon(ConsoleArgs* pconsoleArgs);
    void TermCarbon();

    void InitRecognizer(const std::string& recognizerType, const std::string& wavFileName);
    void InitCommandSystem();

    void WaitForDebugger();

    void RecognizeAsync();
    void ContinuousRecognition(uint16_t seconds);

    void RunSample(const std::string& strSampleName);

    void RunInteractivePrompt();

    void Sample_HelloWorld();
    void Sample_HelloWorld_WithEvents();
    void Sample_HelloWorld_In_C();

    void Sample_HelloWorld_PickEngine(const char* pszEngine);

    void Sample_HelloWorld_Intent();
    void Sample_HelloWorld_Intent(const char* subscriptionKey, const char* appId, const char* region);
    void Sample_HelloWorld_Subscription();
    void Sample_HelloWorld_Subscription_With_CRIS();
    void Sample_HelloWorld_Language(const char* language);

    void Sample_HelloWorld_Kws();

    int Sample_Do_Channel9();

    int channel9();

    void ch9_do_speech();
    void ch9_do_speech_intermediate();
    void ch9_do_speech_continuous();

    void ch9_do_intent();
    void ch9_do_intent_continuous();

    void ch9_do_kws_speech();
    void ch9_do_kws_intent();

    void ch9_do_translation();


private:

    std::shared_ptr<BaseAsyncRecognizer> m_recognizer;
    std::shared_ptr<SpeechRecognizer> m_speechRecognizer;
    std::shared_ptr<TranslationRecognizer> m_translationRecognizer;
    std::shared_ptr<IntentRecognizer> m_intentRecognizer;
    std::shared_ptr<Session> m_session;
    std::string m_subscriptionKey;
    std::string m_regionId;
    std::string m_endpointUri;
    std::string m_customSpeechModelId;
    std::string m_intentAppId;
    std::vector<std::string> m_intentNames;

    void* m_commandSystem = nullptr;
};
