// <copyright file="ByosAudioUploadedEnvironmentVariables.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosAudioUploaded
{
    using System;
    using Connector;
    using Connector.Constants;

    public static class ByosAudioUploadedEnvironmentVariables
    {
        public static readonly string AzureServiceBus = Environment.GetEnvironmentVariable(nameof(AzureServiceBus), EnvironmentVariableTarget.Process);

        public static readonly string AzureWebJobsStorage = Environment.GetEnvironmentVariable(nameof(AzureWebJobsStorage), EnvironmentVariableTarget.Process);

        public static readonly string AudioUploadedServiceBusConnectionString = Environment.GetEnvironmentVariable(nameof(AudioUploadedServiceBusConnectionString), EnvironmentVariableTarget.Process);

        // Azure function custom setup:
        public static readonly bool DeleteProcessedAudioFilesFromStorage = bool.TryParse(Environment.GetEnvironmentVariable(nameof(DeleteProcessedAudioFilesFromStorage), EnvironmentVariableTarget.Process), out DeleteProcessedAudioFilesFromStorage) && DeleteProcessedAudioFilesFromStorage;

        public static readonly int MessagesPerFunctionExecution = int.TryParse(Environment.GetEnvironmentVariable(nameof(MessagesPerFunctionExecution), EnvironmentVariableTarget.Process), out MessagesPerFunctionExecution) ? MessagesPerFunctionExecution.ClampInt(1, Constants.MaxMessagesPerFunctionExecution) : Constants.DefaultMessagesPerFunctionExecution;

        public static readonly int FilesPerTranscriptionJob = int.TryParse(Environment.GetEnvironmentVariable(nameof(FilesPerTranscriptionJob), EnvironmentVariableTarget.Process), out FilesPerTranscriptionJob) ? FilesPerTranscriptionJob.ClampInt(1, Constants.MaxFilesPerTranscriptionJob) : Constants.DefaultFilesPerTranscriptionJob;

        public static readonly int RetryLimit = int.TryParse(Environment.GetEnvironmentVariable(nameof(RetryLimit), EnvironmentVariableTarget.Process), out RetryLimit) ? RetryLimit.ClampInt(1, Constants.MaxRetryLimit) : Constants.DefaultRetryLimit;

        public static readonly int InitialRetryDelayInMinutes = int.TryParse(Environment.GetEnvironmentVariable(nameof(InitialRetryDelayInMinutes), EnvironmentVariableTarget.Process), out InitialRetryDelayInMinutes) ? InitialRetryDelayInMinutes.ClampInt(0, Constants.MaxInitialRetryDelayInMinutes) : Constants.DefaultInitialRetryDelayInMinutes;

        public static readonly int MaxRetryDelayInMinutes = int.TryParse(Environment.GetEnvironmentVariable(nameof(MaxRetryDelayInMinutes), EnvironmentVariableTarget.Process), out MaxRetryDelayInMinutes) ? MaxRetryDelayInMinutes.ClampInt(0, Constants.MaxInitialRetryDelayInMinutes) : Constants.DefaultMaxRetryDelayInMinutes;

        // Cognitive services keys setup:
        public static readonly string CognitiveServicesKey = Environment.GetEnvironmentVariable(nameof(CognitiveServicesKey), EnvironmentVariableTarget.Process);

        public static readonly string CognitiveServicesRegion = Environment.GetEnvironmentVariable(nameof(CognitiveServicesRegion), EnvironmentVariableTarget.Process);

        public static readonly string CustomModelId = Environment.GetEnvironmentVariable(nameof(CustomModelId), EnvironmentVariableTarget.Process);

        public static readonly string CognitiveServicesFallbackKey = Environment.GetEnvironmentVariable(nameof(CognitiveServicesFallbackKey), EnvironmentVariableTarget.Process);

        public static readonly string CognitiveServicesFallbackRegion = Environment.GetEnvironmentVariable(nameof(CognitiveServicesFallbackRegion), EnvironmentVariableTarget.Process);

        public static readonly string FallbackCustomModelId = Environment.GetEnvironmentVariable(nameof(FallbackCustomModelId), EnvironmentVariableTarget.Process);

        public static readonly string Locale = Environment.GetEnvironmentVariable(nameof(Locale), EnvironmentVariableTarget.Process);

        // Request properties setup:
        public static readonly bool AddDiarization = bool.TryParse(Environment.GetEnvironmentVariable(nameof(AddDiarization), EnvironmentVariableTarget.Process), out AddDiarization) && AddDiarization;

        public static readonly bool AddWordLevelTimestamps = bool.TryParse(Environment.GetEnvironmentVariable(nameof(AddWordLevelTimestamps), EnvironmentVariableTarget.Process), out AddWordLevelTimestamps) && AddWordLevelTimestamps;

        public static readonly string ProfanityFilterMode = Environment.GetEnvironmentVariable(nameof(ProfanityFilterMode), EnvironmentVariableTarget.Process);

        public static readonly string PunctuationMode = Environment.GetEnvironmentVariable(nameof(PunctuationMode), EnvironmentVariableTarget.Process);

        // Storage configuration:
        public static readonly string AudioInputContainer = Environment.GetEnvironmentVariable(nameof(AudioInputContainer), EnvironmentVariableTarget.Process);

        public static readonly string AudioFailedContainer = Environment.GetEnvironmentVariable(nameof(AudioFailedContainer), EnvironmentVariableTarget.Process);

        public static readonly string ErrorFilesOutputContainer = Environment.GetEnvironmentVariable(nameof(ErrorFilesOutputContainer), EnvironmentVariableTarget.Process);

        public static readonly string ErrorReportOutputContainer = Environment.GetEnvironmentVariable(nameof(ErrorReportOutputContainer), EnvironmentVariableTarget.Process);
    }
}
