// <copyright file="ByosResultUploaded.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosResultUploaded
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

    public static class ByosResultUploaded
    {
        [FunctionName("ByosResultUploaded")]
        public static async Task Run([ServiceBusTrigger("result_uploaded_queue", Connection = "AzureServiceBus")] Message message, ILogger logger)
        {
            message = message ?? throw new ArgumentNullException(nameof(message));
            logger = logger ?? throw new ArgumentNullException(nameof(logger));

            logger.LogInformation($"C# ServiceBus queue trigger function processed message: {message.Label}");

            if (message.Body == null || !message.Body.Any())
            {
                logger.LogError($"Message body of message {nameof(message)} is null.");
                return;
            }

            var messageBody = Encoding.UTF8.GetString(message.Body);
            logger.LogInformation($"Received message: SequenceNumber:{message.SystemProperties.SequenceNumber} Body:{messageBody}");

            var serviceBusMessage = JsonConvert.DeserializeObject<ServiceBusMessage>(messageBody);

            var fileUri = serviceBusMessage.Data.Url;
            (var containerName, var fileName) = StorageConnector.GetContainerAndFileNameFromUri(fileUri);
            logger.LogInformation($"Received result file with name {fileName} from container {containerName}.");

            var resultHelper = new ByosTranscriptionResultHelper(logger);
            await resultHelper.ProcessResultFileAsync(containerName, fileName).ConfigureAwait(false);
        }
    }
}
