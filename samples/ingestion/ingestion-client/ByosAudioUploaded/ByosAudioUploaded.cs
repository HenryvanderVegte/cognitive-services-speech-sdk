// <copyright file="ByosAudioUploaded.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosAudioUploaded
{
    using System;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using Connector;
    using Microsoft.Azure.ServiceBus;
    using Microsoft.Azure.WebJobs;
    using Microsoft.Extensions.Logging;
    using Newtonsoft.Json;

    public static class ByosAudioUploaded
    {
        [FunctionName("ByosAudioUploaded")]
        public static async Task Run([ServiceBusTrigger("audio_uploaded_queue", Connection = "AzureServiceBus")] Message message, ILogger logger)
        {
            message = message ?? throw new ArgumentNullException(nameof(message));
            logger = logger ?? throw new ArgumentNullException(nameof(logger));

            logger.LogInformation($"C# ServiceBus queue trigger function processed message: {message.Label}");

            if (message.Body == null || !message.Body.Any())
            {
                logger.LogError($"Message body of message {nameof(message)} is null.");
                return;
            }

            var serviceBusMessage = JsonConvert.DeserializeObject<ServiceBusMessage>(Encoding.UTF8.GetString(message.Body));
            var audioFileName = StorageConnector.GetFileNameFromUri(serviceBusMessage.Data.Url);

            logger.LogInformation($"Received audio with name: {audioFileName}");

            var transcriptionHelper = new ByosTranscriptionHelper(logger);
            await transcriptionHelper.StartBatchTranscriptionJobAsync(serviceBusMessage, audioFileName).ConfigureAwait(false);
        }
    }
}
