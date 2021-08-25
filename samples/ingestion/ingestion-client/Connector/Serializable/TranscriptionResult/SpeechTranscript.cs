// <copyright file="SpeechTranscript.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    using System.Collections.Generic;
    using Newtonsoft.Json;
    using Newtonsoft.Json.Linq;

    public class SpeechTranscript
    {
        public SpeechTranscript(string source, string timestamp, long durationTicks, string duration, IEnumerable<CombinedRecognizedPhrase> combinedRecognizedPhrases, IEnumerable<RecognizedPhrase> recognizedPhrases)
        {
            Source = source;
            Timestamp = timestamp;
            DurationTicks = durationTicks;
            Duration = duration;
            CombinedRecognizedPhrases = combinedRecognizedPhrases;
            RecognizedPhrases = recognizedPhrases;
        }

        [JsonProperty("source")]
        public string Source { get; }

        [JsonProperty("timeStamp")]
        public string Timestamp { get; }

        [JsonProperty("durationTicks")]
        public long DurationTicks { get; }

        [JsonProperty("duration")]
        public string Duration { get; }

        [JsonProperty("combinedRecognizedPhrases")]
        public IEnumerable<CombinedRecognizedPhrase> CombinedRecognizedPhrases { get; }

        [JsonProperty("recognizedPhrases")]
        public IEnumerable<RecognizedPhrase> RecognizedPhrases { get; }

        [JsonProperty("callReasonResponse")]
        public JObject CallReasonResponse { get; private set; }

        [JsonProperty("redactionResponse")]
        public JObject RedactionResponse { get; private set; }

        public void AddCallReasonResponse(JObject callReasonResponse)
        {
            this.CallReasonResponse = callReasonResponse;
        }

        public void AddRedactionResponse(JObject redactionResponse)
        {
            this.RedactionResponse = redactionResponse;
        }
    }
}
