//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

// <toplevel>
#include <speechapi_cxx.h>

using namespace std;
using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Intent;
// </toplevel>

// Intent recognition using microphone.
void IntentRecognitionWithMicrophone()
{
    // <IntentRecognitionWithMicrophone>
    // Creates an instance of a speech factory with specified subscription key
    // and service region. Note that in contrast to other services supported by
    // the Cognitive Service Speech SDK, the Language Understanding service
    // requires a specific subscription key from https://www.luis.ai/.
    // The Language Understanding service calls the required key 'endpoint key'.
    // Once you've obtained it, replace with below with your own Language Understanding subscription key
    // and service region (e.g., "westus").
    auto factory = SpeechFactory::FromSubscription("YourLanguageUnderstandingSubscriptionKey", "YourLanguageUnderstandingServiceRegion");

    // Creates an intent recognizer using microphone as audio input. The default language is "en-us".
    auto recognizer = factory->CreateIntentRecognizer();

    // Creates a Language Understanding model using the app id, and adds specific intents from your model
    auto model = LanguageUnderstandingModel::FromAppId("YourLanguageUnderstandingAppId");
    recognizer->AddIntent("id1", model, "YourLanguageUnderstandingIntentName1");
    recognizer->AddIntent("id2", model, "YourLanguageUnderstandingIntentName2");
    recognizer->AddIntent("any-IntentId-here", model, "YourLanguageUnderstandingIntentName3");

    cout << "Say something...\n";

    // Performs recognition.
    // RecognizeAsync() returns when the first utterance has been recognized, so it is suitable 
    // only for single shot recognition like command or query. For long-running recognition, use
    // StartContinuousRecognitionAsync() instead.
    auto result = recognizer->RecognizeAsync().get();

    // Checks result.
    if (result->Reason != Reason::Recognized)
    {
        cout << "Recognition Status: " << int(result->Reason) << ". ";
        if (result->Reason == Reason::Canceled)
        {
            cout << "There was an error, reason: " << result->ErrorDetails << std::endl;
        }
        else
        {
            cout << "No speech could be recognized.\n";
        }
    }
    else
    {
        cout << "We recognized: " << result->Text << std::endl;
        cout << "    Intent Id: " << result->IntentId << std::endl;
        cout << "    Intent response in Json: " << result->Properties[ResultProperty::LanguageUnderstandingJson].GetString() << std::endl;
    }
    // </IntentRecognitionWithMicrophone>
}

// Intent recognition in the specified language, using microphone.
void IntentRecognitionWithLanguage()
{
    // <IntentRecognitionWithLanguage>
    // Creates an instance of a speech factory with specified subscription key
    // and service region. Note that in contrast to other services supported by
    // the Cognitive Service Speech SDK, the Language Understanding service
    // requires a specific subscription key from https://www.luis.ai/.
    // The Language Understanding service calls the required key 'endpoint key'.
    // Once you've obtained it, replace with below with your own Language Understanding service subscription key
    // and service region (e.g., "westus").
    auto factory = SpeechFactory::FromSubscription("YourLanguageUnderstandingSubscriptionKey", "YourLanguageUnderstandingServiceRegion");

    // Creates an intent recognizer in the specified language using microphone as audio input.
    auto lang = "de-de";
    auto recognizer = factory->CreateIntentRecognizer(lang);

    // Creates a Language Understanding model using the app id, and adds specific intents from your model
    auto model = LanguageUnderstandingModel::FromAppId("YourLanguageUnderstandingAppId");
    recognizer->AddIntent("id1", model, "YourLanguageUnderstandingIntentName1");
    recognizer->AddIntent("id2", model, "YourLanguageUnderstandingIntentName2");
    recognizer->AddIntent("any-IntentId-here", model, "YourLanguageUnderstandingIntentName3");

    cout << "Say something in " << lang << "..." << std::endl;

    // Performs recognition.
    // RecognizeAsync() returns when the first utterance has been recognized, so it is suitable 
    // only for single shot recognition like command or query. For long-running recognition, use
    // StartContinuousRecognitionAsync() instead.
    auto result = recognizer->RecognizeAsync().get();

    // Checks result.
    if (result->Reason != Reason::Recognized)
    {
        cout << "Recognition Status:" << int(result->Reason);
        if (result->Reason == Reason::Canceled)
        {
            cout << "There was an error, reason: " << result->ErrorDetails << std::endl;
        }
        else
        {
            cout << "No speech could be recognized." << std::endl;
        }
    }
    else
    {
        cout << "We recognized: " << result->Text << std::endl;
        cout << "    Intent Id: " << result->IntentId << std::endl;
        cout << "    Intent response in Json: " << result->Properties[ResultProperty::LanguageUnderstandingJson].GetString() << std::endl;
    }
    // </IntentRecognitionWithLanguage>
}

// Continuous intent recognition.
void IntentContinuousRecognitionWithFile()
{
    // <IntentContinuousRecognitionWithFile>
    // Creates an instance of a speech factory with specified subscription key
    // and service region. Note that in contrast to other services supported by
    // the Cognitive Service Speech SDK, the Language Understanding service
    // requires a specific subscription key from https://www.luis.ai/.
    // The Language Understanding service calls the required key 'endpoint key'.
    // Once you've obtained it, replace with below with your own Language Understanding subscription key
    // and service region (e.g., "westus").
    auto factory = SpeechFactory::FromSubscription("YourLanguageUnderstandingSubscriptionKey", "YourLanguageUnderstandingServiceRegion");

    // Creates an intent recognizer using file as audio input.
    // Replace with your own audio file name.
    auto recognizer = factory->CreateIntentRecognizerWithFileInput("whatstheweatherlike.wav");

    // promise for synchronization of recognition end.
    std::promise<void> recognitionEnd;

    // Creates a Language Understanding model using the app id, and adds specific intents from your model
    auto model = LanguageUnderstandingModel::FromAppId("YourLanguageUnderstandingAppId");
    recognizer->AddIntent("id1", model, "YourLanguageUnderstandingIntentName1");
    recognizer->AddIntent("id2", model, "YourLanguageUnderstandingIntentName2");
    recognizer->AddIntent("any-IntentId-here", model, "YourLanguageUnderstandingIntentName3");

    // Subscribes to events.
    recognizer->IntermediateResult.Connect([] (const IntentRecognitionEventArgs& e)
    {
        cout << "IntermediateResult:" << e.Result.Text << std::endl;
    });

    recognizer->FinalResult.Connect([] (const IntentRecognitionEventArgs& e)
    {
        cout << "FinalResult: status:" << (int)e.Result.Reason << ". Text: " << e.Result.Text << std::endl;
        cout << "    Intent Id: " << e.Result.IntentId << std::endl;
        cout << "    Language Understanding Json: " << e.Result.Properties[ResultProperty::LanguageUnderstandingJson].GetString() << std::endl;
    });

    recognizer->Canceled.Connect([&recognitionEnd] (const IntentRecognitionEventArgs& e)
    {
        cout << "Canceled:" << (int)e.Result.Reason << "- " << e.Result.ErrorDetails << std::endl;
        // Notify to stop recognition.
        recognitionEnd.set_value();
    });

    recognizer->SessionStopped.Connect([&recognitionEnd](const SessionEventArgs& e)
    {
        cout << "Session stopped.";
        // Notify to stop recognition.
        recognitionEnd.set_value();
    });

    // Starts continuous recognition. Uses StopContinuousRecognitionAsync() to stop recognition.
    recognizer->StartContinuousRecognitionAsync().wait();

    // Waits for recognition end.
    recognitionEnd.get_future().wait();

    // Stops recognition.
    recognizer->StopContinuousRecognitionAsync().wait();
    // </IntentContinuousRecognitionWithFile>
}
