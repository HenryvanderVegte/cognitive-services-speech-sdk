// <copyright file="BatchClient.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    using System;
    using System.Collections.Generic;
    using System.Net;
    using System.Net.Http;
    using System.Text;
    using System.Threading;
    using System.Threading.Tasks;
    using Connector.Extensions;
    using Connector.Serializable.TranscriptionFiles;
    using Microsoft.Extensions.Logging;
    using Newtonsoft.Json;

    public static class BatchClient
    {
        private const string TranscriptionsBasePath = "speechtotext/v3.0/Transcriptions/";

        private static readonly TimeSpan PostTimeout = TimeSpan.FromMinutes(1);

        private static readonly TimeSpan DefaultTimeout = TimeSpan.FromMinutes(2);

        private static readonly TimeSpan GetFilesTimeout = TimeSpan.FromMinutes(5);

        private static readonly HttpClient HttpClient = new () { Timeout = Timeout.InfiniteTimeSpan };

        public static Task<TranscriptionReportFile> GetTranscriptionReportFileFromSasAsync(string sasUri, ILogger log)
        {
            return GetAsync<TranscriptionReportFile>(sasUri, null, DefaultTimeout, log);
        }

        public static Task<SpeechTranscript> GetSpeechTranscriptFromSasAsync(string sasUri, ILogger log)
        {
            return GetAsync<SpeechTranscript>(sasUri, null, DefaultTimeout, log);
        }

        public static Task<Transcription> GetTranscriptionAsync(string transcriptionLocation, string subscriptionKey, ILogger log)
        {
            return GetAsync<Transcription>(transcriptionLocation, subscriptionKey, DefaultTimeout, log);
        }

        public static async Task<TranscriptionFiles> GetTranscriptionFilesAsync(string transcriptionLocation, string subscriptionKey, ILogger log)
        {
            var path = $"{transcriptionLocation}/files";
            var combinedTranscriptionFiles = new List<TranscriptionFile>();

            do
            {
                var transcriptionFiles = await GetAsync<TranscriptionFiles>(path, subscriptionKey, GetFilesTimeout, log).ConfigureAwait(false);
                combinedTranscriptionFiles.AddRange(transcriptionFiles.Values);
                path = transcriptionFiles.NextLink;
            }
            while (!string.IsNullOrEmpty(path));

            return new TranscriptionFiles(combinedTranscriptionFiles, null);
        }

        public static Task DeleteTranscriptionAsync(string transcriptionLocation, string subscriptionKey, ILogger log)
        {
            return DeleteAsync(transcriptionLocation, subscriptionKey, DefaultTimeout, log);
        }

        public static async Task<Uri> PostTranscriptionAsync(TranscriptionDefinition transcriptionDefinition, string hostName, string subscriptionKey, ILogger log)
        {
            var path = $"{hostName}{TranscriptionsBasePath}";
            var payloadString = JsonConvert.SerializeObject(transcriptionDefinition);

            return await PostAsync(path, subscriptionKey, payloadString, PostTimeout, log).ConfigureAwait(false);
        }

        private static async Task<Uri> PostAsync(string path, string subscriptionKey, string payloadString, TimeSpan timeout, ILogger log)
        {
            using var requestMessage = new HttpRequestMessage(HttpMethod.Post, path);
            requestMessage.Headers.Add("Ocp-Apim-Subscription-Key", subscriptionKey);
            requestMessage.Content = new StringContent(payloadString, Encoding.UTF8, "application/json");

            var responseMessage = await SendHttpRequestMessage(requestMessage, timeout, log).ConfigureAwait(false);

            return responseMessage.Headers.Location;
        }

        private static async Task DeleteAsync(string path, string subscriptionKey, TimeSpan timeout, ILogger log)
        {
            using var requestMessage = new HttpRequestMessage(HttpMethod.Delete, path);
            if (!string.IsNullOrEmpty(subscriptionKey))
            {
                requestMessage.Headers.Add("Ocp-Apim-Subscription-Key", subscriptionKey);
            }

            await SendHttpRequestMessage(requestMessage, timeout, log).ConfigureAwait(false);
        }

        private static async Task<TResponse> GetAsync<TResponse>(string path, string subscriptionKey, TimeSpan timeout, ILogger log)
        {
            using var requestMessage = new HttpRequestMessage(HttpMethod.Get, path);
            if (!string.IsNullOrEmpty(subscriptionKey))
            {
                requestMessage.Headers.Add("Ocp-Apim-Subscription-Key", subscriptionKey);
            }

            var responseMessage = await SendHttpRequestMessage(requestMessage, timeout, log).ConfigureAwait(false);

            var contentString = await responseMessage.Content.ReadAsStringAsync().ConfigureAwait(false);
            return JsonConvert.DeserializeObject<TResponse>(contentString);
        }

        private static async Task<HttpResponseMessage> SendHttpRequestMessage(HttpRequestMessage httpRequestMessage, TimeSpan timeout, ILogger log)
        {
            try
            {
                using var cts = new CancellationTokenSource();
                cts.CancelAfter(timeout);
                var responseMessage = await HttpClient.SendAsync(httpRequestMessage, cts.Token).ConfigureAwait(false);

                if (IsThrottledOrTimeoutStatusCode(responseMessage.StatusCode))
                {
                    throw new TimeoutException(responseMessage.StatusCode.ToString());
                }

                await responseMessage.EnsureSuccessStatusCodeAsync().ConfigureAwait(false);

                return responseMessage;
            }
            catch (OperationCanceledException)
            {
                throw new TimeoutException($"The operation has timed out after {timeout.TotalSeconds} seconds.");
            }
        }
    }
}
