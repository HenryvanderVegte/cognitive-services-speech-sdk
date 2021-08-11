// <copyright file="ByosTranscriptionResultHelper.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosResultUploaded
{
    using System;
    using System.Text;
    using System.Text.Json;
    using System.Threading.Tasks;
    using Connector;
    using Microsoft.Extensions.Logging;

    public sealed class ByosTranscriptionResultHelper
    {
        private static readonly StorageConnector StorageConnectorInstance = new StorageConnector(ByosResultUploadedEnvironmentVariables.AzureWebJobsStorage);

        private readonly ILogger Logger;

        public ByosTranscriptionResultHelper(ILogger logger)
        {
            Logger = logger;
        }

        public async Task ProcessResultFileAsync(string artifactContainerName, string artifactFileName)
        {
            var byteResult = await StorageConnectorInstance.DownloadFileFromContainerAsync(artifactContainerName, artifactFileName).ConfigureAwait(false);
            var jsonResult = Encoding.UTF8.GetString(byteResult);

            var jsonDocument = JsonDocument.Parse(jsonResult);

            // Check whether file is report file or transcription file:
            if (jsonDocument.RootElement.TryGetProperty("successfulTranscriptionsCount", out var successfulTranscriptionsElement) &&
                successfulTranscriptionsElement.TryGetInt32(out var successfulTranscriptionsCount) &&
                jsonDocument.RootElement.TryGetProperty("failedTranscriptionsCount", out var failedTranscriptionsElement) &&
                failedTranscriptionsElement.TryGetInt32(out var failedTranscriptionsCount))
            {
                Logger.LogInformation($"Received report file with {successfulTranscriptionsCount} succeeded and {failedTranscriptionsCount} failed transcriptions.");

                // No need to process report file if all transcriptions are succeeded as succeeded results are processed independently
                if (failedTranscriptionsCount != 0)
                {
                    await ProcessReportFileAsync(jsonDocument.RootElement).ConfigureAwait(false);
                }

                if (ByosResultUploadedEnvironmentVariables.DeleteCustomSpeechArtifacts)
                {
                    await StorageConnectorInstance.DeleteFileAsync(artifactContainerName, artifactFileName, Logger).ConfigureAwait(false);
                }
            }
            else if (jsonDocument.RootElement.TryGetProperty("source", out _) &&
                jsonDocument.RootElement.TryGetProperty("combinedRecognizedPhrases", out _) &&
                jsonDocument.RootElement.TryGetProperty("recognizedPhrases", out _))
            {
                await ProcessTranscriptionFileAsync(jsonDocument.RootElement, jsonResult).ConfigureAwait(false);

                if (ByosResultUploadedEnvironmentVariables.DeleteCustomSpeechArtifacts)
                {
                    await StorageConnectorInstance.DeleteFileAsync(artifactContainerName, artifactFileName, Logger).ConfigureAwait(false);
                }
            }
            else
            {
                // Leave custom speech artifact on storage in that case for investigation
                Logger.LogError($"Unexpected result file format for file {artifactFileName} in container {artifactContainerName}.");
            }
        }

        private async Task ProcessTranscriptionFileAsync(JsonElement audioFileRoot, string transcriptionJson)
        {
            var sourceUri = audioFileRoot.GetProperty("source").GetString();
            var audioFileName = StorageConnector.GetFileNameFromUri(new Uri(sourceUri));

            await StorageConnectorInstance.WriteTextFileToBlobAsync(
                transcriptionJson,
                ByosResultUploadedEnvironmentVariables.JsonResultOutputContainer,
                $"{audioFileName}.json",
                Logger).ConfigureAwait(false);

            Logger.LogInformation($"Writing transcription file with name {audioFileName}.json to results container");

            if (ByosResultUploadedEnvironmentVariables.DeleteProcessedAudioFilesFromStorage)
            {
                await StorageConnectorInstance.DeleteFileAsync(
                    ByosResultUploadedEnvironmentVariables.AudioInputContainer,
                    audioFileName,
                    Logger).ConfigureAwait(false);
            }
            else
            {
                await StorageConnectorInstance.MoveFileAsync(
                    ByosResultUploadedEnvironmentVariables.AudioInputContainer,
                    audioFileName,
                    ByosResultUploadedEnvironmentVariables.AudioProcessedContainer,
                    audioFileName,
                    false,
                    Logger).ConfigureAwait(false);
            }
        }

        private async Task ProcessReportFileAsync(JsonElement reportFileRoot)
        {
            foreach (var detail in reportFileRoot.GetProperty("details").EnumerateArray())
            {
                // Only process failed transcriptions as succeeded results are processed independently
                if (detail.TryGetProperty("status", out var statusProperty) && statusProperty.GetString().Equals("Failed", StringComparison.OrdinalIgnoreCase))
                {
                    var sourceUri = new Uri(detail.GetProperty("source").GetString());
                    (var containerName, var fileName) = StorageConnector.GetContainerAndFileNameFromUri(sourceUri);

                    var errorMessage = detail.TryGetProperty("errorMessage", out var errorMessageProperty) ? errorMessageProperty.GetString() : "Unknown";
                    var errorKind = detail.TryGetProperty("errorKind", out var errorKindProperty) ? errorKindProperty.GetString() : "Unknown";

                    var combinedErrorMessage = $"Transcription {fileName} in container {containerName} failed with error \"{errorMessage}\" ({errorKind}).";
                    Logger.LogWarning(combinedErrorMessage);

                    await StorageConnectorInstance.WriteTextFileToBlobAsync(
                        combinedErrorMessage,
                        ByosResultUploadedEnvironmentVariables.ErrorReportOutputContainer,
                        fileName,
                        Logger).ConfigureAwait(false);

                    if (ByosResultUploadedEnvironmentVariables.DeleteProcessedAudioFilesFromStorage)
                    {
                        await StorageConnectorInstance.DeleteFileAsync(
                            ByosResultUploadedEnvironmentVariables.AudioInputContainer,
                            $"{fileName}.txt",
                            Logger).ConfigureAwait(false);
                    }
                    else
                    {
                        await StorageConnectorInstance.MoveFileAsync(
                            ByosResultUploadedEnvironmentVariables.AudioInputContainer,
                            fileName,
                            ByosResultUploadedEnvironmentVariables.AudioFailedContainer,
                            fileName,
                            false,
                            Logger).ConfigureAwait(false);
                    }
                }
            }
        }
    }
}
