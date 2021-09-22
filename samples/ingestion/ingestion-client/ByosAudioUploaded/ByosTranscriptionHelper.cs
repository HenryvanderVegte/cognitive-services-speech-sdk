// <copyright file="ByosTranscriptionHelper.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosAudioUploaded
{
    using System;
    using System.Collections.Generic;
    using System.Globalization;
    using System.Linq;
    using System.Net;
    using System.Net.Http;
    using System.Text;
    using System.Threading.Tasks;
    using Connector;
    using Connector.Serializable.TranscriptionStartedServiceBusMessage;
    using Microsoft.Azure.ServiceBus;
    using Microsoft.Extensions.Logging;
    using Microsoft.WindowsAzure.Storage;
    using Newtonsoft.Json;

    public sealed class ByosTranscriptionHelper
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Performance", "CA1823:Avoid unused private fields", Justification = "Calls for DummyCallbackClient still need to be implemented.")]
        private static readonly DummyCallbackClient DummyCallbackClient = new DummyCallbackClient(
            new HttpClient(),
            ByosAudioUploadedEnvironmentVariables.ClientId,
            ByosAudioUploadedEnvironmentVariables.ClientSecret,
            ByosAudioUploadedEnvironmentVariables.Scope,
            new Uri(ByosAudioUploadedEnvironmentVariables.TokenEndpointUrl),
            new Uri(ByosAudioUploadedEnvironmentVariables.CallbackBaseUrl));

        private static readonly StorageConnector StorageConnectorInstance = new StorageConnector(ByosAudioUploadedEnvironmentVariables.AzureWebJobsStorage);

        private static readonly QueueClient AudioUploadedQueueClientInstance = new QueueClient(new ServiceBusConnectionStringBuilder(ByosAudioUploadedEnvironmentVariables.AudioUploadedServiceBusConnectionString));

        private static Random Random = new Random();

        private readonly ILogger Logger;

        private readonly string Locale;

        public ByosTranscriptionHelper(ILogger logger)
        {
            Logger = logger;
            Locale = ByosAudioUploadedEnvironmentVariables.Locale.Split('|')[0].Trim();
        }

        public async Task StartBatchTranscriptionJobAsync(ServiceBusMessage serviceBusMessage, string fileName)
        {
            serviceBusMessage = serviceBusMessage ?? throw new ArgumentNullException(nameof(serviceBusMessage));

            // Select a random subscription based on the request split percentages defined in the ARM template:
            var subscriptionIndex = GetRandomSubscriptionIndex();

            var cognitiveServicesKey = ByosAudioUploadedEnvironmentVariables.CognitiveServicesKeys[subscriptionIndex];
            var cognitiveServicesRegion = ByosAudioUploadedEnvironmentVariables.CognitiveServicesRegions[subscriptionIndex];
            var cognitiveServicesModelId = ByosAudioUploadedEnvironmentVariables.CustomModelIds[subscriptionIndex];

            Logger.LogInformation($"Subscription at index {subscriptionIndex} selected, sending job {fileName} to region {cognitiveServicesRegion}, retry: {serviceBusMessage.RetryCount}");

            /***
             * TODO:
             * Send status update via DummyCallbackClient: "Audio file was picked up by Ingestion Client - region: {cognitiveServicesRegion}."
             */

            await SubmitTranscriptionJob(
                serviceBusMessage,
                fileName,
                cognitiveServicesKey,
                cognitiveServicesRegion,
                cognitiveServicesModelId).ConfigureAwait(false);
        }

        private static TimeSpan GetMessageDelayTime(int pollingCounter)
        {
            if (pollingCounter == 0)
            {
                return TimeSpan.FromMinutes(ByosAudioUploadedEnvironmentVariables.InitialRetryDelayInMinutes);
            }

            var updatedDelay = Math.Pow(2, Math.Min(pollingCounter, 8)) * ByosAudioUploadedEnvironmentVariables.InitialRetryDelayInMinutes;

            if ((int)updatedDelay > ByosAudioUploadedEnvironmentVariables.MaxRetryDelayInMinutes)
            {
                return TimeSpan.FromMinutes(ByosAudioUploadedEnvironmentVariables.MaxRetryDelayInMinutes);
            }

            return TimeSpan.FromMinutes(updatedDelay);
        }

        private static bool HasStatusCode(Exception exception, out HttpStatusCode httpStatusCode)
        {
            httpStatusCode = HttpStatusCode.NotFound;

            if (exception is TimeoutException)
            {
                httpStatusCode = HttpStatusCode.RequestTimeout;
                return true;
            }

            if (exception is HttpStatusCodeException statusCodeException && statusCodeException.HttpStatusCode.HasValue)
            {
                httpStatusCode = statusCodeException.HttpStatusCode.Value;
                return true;
            }

            if (exception is WebException webException && webException.Response != null)
            {
                httpStatusCode = ((HttpWebResponse)webException.Response).StatusCode;
                return true;
            }

            return false;
        }

        private static int GetRandomSubscriptionIndex()
        {
            var threshold = Random.Next(100);

            var id = 0;
            var percentageSum = 0;

            foreach (var requestPercentage in ByosAudioUploadedEnvironmentVariables.RequestPercentages)
            {
                percentageSum += requestPercentage;
                if (threshold < percentageSum)
                {
                    return id;
                }

                id++;
            }

            return 0;
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1031:Do not catch general exception types", Justification = "Catch general exception to ensure that job continues execution even if message is invalid.")]
        private async Task SubmitTranscriptionJob(
            ServiceBusMessage serviceBusMessage,
            string jobName,
            string cognitiveServicesKey,
            string cognitiveServicesRegion,
            string customModelId)
        {
            var hostName = $"https://{cognitiveServicesRegion}.api.cognitive.microsoft.com/";

            try
            {
                var properties = GetTranscriptionPropertyBag();

                var sasUrls = new List<string>();
                var audioFileInfos = new List<AudioFileInfo>();

                var sasUrl = StorageConnectorInstance.CreateSas(serviceBusMessage.Data.Url);
                sasUrls.Add(sasUrl);
                audioFileInfos.Add(new AudioFileInfo(serviceBusMessage.Data.Url.AbsoluteUri, serviceBusMessage.RetryCount));

                ModelIdentity modelIdentity = null;
                if (Guid.TryParse(customModelId, out var customModelGuid))
                {
                    modelIdentity = ModelIdentity.Create(cognitiveServicesRegion, customModelGuid);
                }

                var transcriptionDefinition = TranscriptionDefinition.Create(jobName, "StartByTimerTranscription", Locale, sasUrls, properties, modelIdentity);

                var transcriptionLocation = await BatchClient.PostTranscriptionAsync(
                    transcriptionDefinition,
                    hostName,
                    cognitiveServicesKey).ConfigureAwait(false);

                /***
                 * TODO:
                 * Send status update via DummyCallbackClient: "Audio file was successfully sent to speech service - location: {transcriptionLocation}."
                 */

                Logger.LogInformation($"Submitted audio files {string.Join(", ", audioFileInfos.Select(a => StorageConnector.GetFileNameFromUri(new Uri(a.FileUrl))))} in transcription job: {transcriptionLocation}");
            }
            catch (Exception exception)
            {
                if (HasStatusCode(exception, out var httpStatusCode) && httpStatusCode.IsRetryableStatus())
                {
                    await RetryOrFailMessageAsync(serviceBusMessage, $"Error in job {jobName}: {exception.Message}, status code: {httpStatusCode}").ConfigureAwait(false);
                }
                else
                {
                    await WriteFailedJobLogToStorageAsync(serviceBusMessage, $"Exception {exception} in job {jobName}: {exception.Message}", jobName).ConfigureAwait(false);
                }
            }

            Logger.LogInformation($"Completed processing of job {jobName}.");
        }

        private async Task RetryOrFailMessageAsync(ServiceBusMessage serviceBusMessage, string errorMessage)
        {
            Logger.LogError(errorMessage);
            if (serviceBusMessage.RetryCount <= ByosAudioUploadedEnvironmentVariables.RetryLimit)
            {
                serviceBusMessage.RetryCount += 1;
                var messageDelay = GetMessageDelayTime(serviceBusMessage.RetryCount);
                var newMessage = new Message(Encoding.UTF8.GetBytes(JsonConvert.SerializeObject(serviceBusMessage)));

                /***
                 * TODO:
                 * Send status update via DummyCallbackClient: "Audio file failed, retrying... - error: {errorMessage}."
                 */

                await ServiceBusUtilities.SendServiceBusMessageAsync(AudioUploadedQueueClientInstance, newMessage, Logger, messageDelay).ConfigureAwait(false);
            }
            else
            {
                var fileName = StorageConnector.GetFileNameFromUri(serviceBusMessage.Data.Url);
                var errorFileName = fileName + ".txt";
                var retryExceededErrorMessage = $"Exceeded retry count for transcription {fileName} with error message {errorMessage}.";
                Logger.LogError(retryExceededErrorMessage);

                /***
                 * TODO:
                 * Send status update via DummyCallbackClient: "Audio file failed, no more retrying - error: {errorMessage}."
                 */

                await ProcessFailedFileAsync(fileName, errorMessage, errorFileName).ConfigureAwait(false);
            }
        }

        private async Task WriteFailedJobLogToStorageAsync(ServiceBusMessage serviceBusMessage, string errorMessage, string jobName)
        {
            Logger.LogError(errorMessage);
            var jobErrorFileName = $"jobs/{jobName}.txt";
            await StorageConnectorInstance.WriteTextFileToBlobAsync(
                errorMessage,
                ByosAudioUploadedEnvironmentVariables.ErrorReportOutputContainer,
                jobErrorFileName,
                Logger).ConfigureAwait(false);

            var fileName = StorageConnector.GetFileNameFromUri(serviceBusMessage.Data.Url);
            var errorFileName = fileName + ".txt";
            await ProcessFailedFileAsync(fileName, errorMessage, errorFileName).ConfigureAwait(false);
        }

        private Dictionary<string, string> GetTranscriptionPropertyBag()
        {
            var properties = new Dictionary<string, string>();

            var profanityFilterMode = ByosAudioUploadedEnvironmentVariables.ProfanityFilterMode;
            properties.Add("ProfanityFilterMode", profanityFilterMode);
            Logger.LogInformation($"Setting profanity filter mode to {profanityFilterMode}");

            var punctuationMode = ByosAudioUploadedEnvironmentVariables.PunctuationMode;
            punctuationMode = punctuationMode.Replace(" ", string.Empty, StringComparison.Ordinal);
            properties.Add("PunctuationMode", punctuationMode);
            Logger.LogInformation($"Setting punctuation mode to {punctuationMode}");

            var addDiarization = ByosAudioUploadedEnvironmentVariables.AddDiarization;
            properties.Add("DiarizationEnabled", addDiarization.ToString(CultureInfo.InvariantCulture));
            Logger.LogInformation($"Setting diarization enabled to {addDiarization}");

            var addWordLevelTimestamps = ByosAudioUploadedEnvironmentVariables.AddWordLevelTimestamps;
            properties.Add("WordLevelTimestampsEnabled", addWordLevelTimestamps.ToString(CultureInfo.InvariantCulture));
            Logger.LogInformation($"Setting word level timestamps enabled to {addWordLevelTimestamps}");

            var transcriptionTimeToLive = ByosAudioUploadedEnvironmentVariables.TranscriptionTimeToLive;
            if (!string.IsNullOrEmpty(transcriptionTimeToLive))
            {
                properties.Add("timeToLive", transcriptionTimeToLive);
                Logger.LogInformation($"Setting transcription time to live to \'{transcriptionTimeToLive}\'");
            }

            return properties;
        }

        private async Task ProcessFailedFileAsync(string fileName, string errorMessage, string logFileName)
        {
            try
            {
                await StorageConnectorInstance.WriteTextFileToBlobAsync(
                    errorMessage,
                    ByosAudioUploadedEnvironmentVariables.ErrorReportOutputContainer,
                    logFileName,
                    Logger).ConfigureAwait(false);

                if (ByosAudioUploadedEnvironmentVariables.DeleteProcessedAudioFilesFromStorage)
                {
                    await StorageConnectorInstance.DeleteFileAsync(
                        ByosAudioUploadedEnvironmentVariables.AudioInputContainer,
                        fileName,
                        Logger).ConfigureAwait(false);
                }
                else
                {
                    await StorageConnectorInstance.MoveFileAsync(
                        ByosAudioUploadedEnvironmentVariables.AudioInputContainer,
                        fileName,
                        ByosAudioUploadedEnvironmentVariables.ErrorFilesOutputContainer,
                        fileName,
                        false,
                        Logger).ConfigureAwait(false);
                }
            }
            catch (StorageException e)
            {
                Logger.LogError($"Storage Exception {e} while writing error log to file and moving result");
            }
        }
    }
}
