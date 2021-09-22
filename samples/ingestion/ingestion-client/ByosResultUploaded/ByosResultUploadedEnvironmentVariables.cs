// <copyright file="ByosResultUploadedEnvironmentVariables.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosResultUploaded
{
    using System;

    public static class ByosResultUploadedEnvironmentVariables
    {
        public static readonly string AzureWebJobsStorage = Environment.GetEnvironmentVariable(nameof(AzureWebJobsStorage), EnvironmentVariableTarget.Process);

        // Azure function custom setup:
        public static readonly bool DeleteProcessedAudioFilesFromStorage = bool.TryParse(Environment.GetEnvironmentVariable(nameof(DeleteProcessedAudioFilesFromStorage), EnvironmentVariableTarget.Process), out DeleteProcessedAudioFilesFromStorage) && DeleteProcessedAudioFilesFromStorage;

        public static readonly bool DeleteCustomSpeechArtifacts = bool.TryParse(Environment.GetEnvironmentVariable(nameof(DeleteCustomSpeechArtifacts), EnvironmentVariableTarget.Process), out DeleteCustomSpeechArtifacts) && DeleteCustomSpeechArtifacts;

        // Storage configuration:
        public static readonly string AudioInputContainer = Environment.GetEnvironmentVariable(nameof(AudioInputContainer), EnvironmentVariableTarget.Process);

        public static readonly string AudioFailedContainer = Environment.GetEnvironmentVariable(nameof(AudioFailedContainer), EnvironmentVariableTarget.Process);

        public static readonly string AudioProcessedContainer = Environment.GetEnvironmentVariable(nameof(AudioProcessedContainer), EnvironmentVariableTarget.Process);

        public static readonly string ErrorReportOutputContainer = Environment.GetEnvironmentVariable(nameof(ErrorReportOutputContainer), EnvironmentVariableTarget.Process);

        public static readonly string JsonResultOutputContainer = Environment.GetEnvironmentVariable(nameof(JsonResultOutputContainer), EnvironmentVariableTarget.Process);

        public static readonly string SpeechServicesOutputContainer = Environment.GetEnvironmentVariable(nameof(SpeechServicesOutputContainer), EnvironmentVariableTarget.Process);

        // Callback API configuration:
        public static readonly string ClientId = Environment.GetEnvironmentVariable(nameof(ClientId), EnvironmentVariableTarget.Process);

        public static readonly string ClientSecret = Environment.GetEnvironmentVariable(nameof(ClientSecret), EnvironmentVariableTarget.Process);

        public static readonly string TokenEndpointUrl = Environment.GetEnvironmentVariable(nameof(TokenEndpointUrl), EnvironmentVariableTarget.Process);

        public static readonly string Scope = Environment.GetEnvironmentVariable(nameof(Scope), EnvironmentVariableTarget.Process);

        public static readonly string CallbackBaseUrl = Environment.GetEnvironmentVariable(nameof(CallbackBaseUrl), EnvironmentVariableTarget.Process);
    }
}
