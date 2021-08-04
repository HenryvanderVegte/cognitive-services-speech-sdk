// <copyright file="ByosAudioUploaded.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace ByosAudioUploaded
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using Connector;
    using Microsoft.Azure.ServiceBus;
    using Microsoft.Azure.ServiceBus.Core;
    using Microsoft.Azure.WebJobs;
    using Microsoft.Extensions.Logging;
    using Newtonsoft.Json;

    public static class ByosAudioUploaded
    {
        private const double MessageReceiveTimeoutInSeconds = 30;

        private static readonly MessageReceiver MessageReceiverInstance = new (new ServiceBusConnectionStringBuilder(ByosAudioUploadedEnvironmentVariables.AudioUploadedServiceBusConnectionString), prefetchCount: ByosAudioUploadedEnvironmentVariables.MessagesPerFunctionExecution);

        [FunctionName("ByosAudioUploaded")]
        public static async Task Run([TimerTrigger("0 */1 * * * *")] TimerInfo timerInfo, ILogger logger)
        {
            logger = logger ?? throw new ArgumentNullException(nameof(logger));
            timerInfo = timerInfo ?? throw new ArgumentNullException(nameof(timerInfo));

            var startDateTime = DateTime.UtcNow;
            logger.LogInformation($"C# Timer trigger function v3 executed at: {startDateTime}. Next occurrence on {timerInfo.Schedule.GetNextOccurrence(startDateTime)}.");

            var validServiceBusMessages = new List<Message>();

            logger.LogInformation("Pulling messages from queue...");
            var messages = await MessageReceiverInstance.ReceiveAsync(ByosAudioUploadedEnvironmentVariables.MessagesPerFunctionExecution, TimeSpan.FromSeconds(MessageReceiveTimeoutInSeconds)).ConfigureAwait(false);

            if (messages == null || !messages.Any())
            {
                logger.LogInformation($"Got no messages in this iteration.");
                return;
            }

            logger.LogInformation($"Got {messages.Count} in this iteration.");
            foreach (var message in messages)
            {
                if (message.SystemProperties.LockedUntilUtc > DateTime.UtcNow.AddSeconds(5))
                {
                    try
                    {
                        if (IsValidServiceBusMessage(message, logger))
                        {
                            await MessageReceiverInstance.RenewLockAsync(message.SystemProperties.LockToken).ConfigureAwait(false);
                            validServiceBusMessages.Add(message);
                        }
                        else
                        {
                            await MessageReceiverInstance.CompleteAsync(message.SystemProperties.LockToken).ConfigureAwait(false);
                        }
                    }
                    catch (MessageLockLostException)
                    {
                        logger.LogInformation($"Message lock expired for message. Ignore message in this iteration.");
                    }
                }
            }

            if (!validServiceBusMessages.Any())
            {
                logger.LogInformation("No valid messages were found in this function execution.");
                return;
            }

            logger.LogInformation($"Pulled {validServiceBusMessages.Count} valid messages from queue.");

            var byosTranscriptionHelper = new ByosTranscriptionHelper(logger);
            await byosTranscriptionHelper.StartTranscriptionsAsync(validServiceBusMessages, MessageReceiverInstance, startDateTime).ConfigureAwait(false);
        }

        public static bool IsValidServiceBusMessage(Message message, ILogger logger)
        {
            if (message == null || message.Body == null || !message.Body.Any())
            {
                logger.LogError($"Message {nameof(message)} is null.");
                return false;
            }

            var messageBody = Encoding.UTF8.GetString(message.Body);

            try
            {
                var serviceBusMessage = JsonConvert.DeserializeObject<ServiceBusMessage>(messageBody);

                if (serviceBusMessage.EventType.Contains("BlobCreate", StringComparison.OrdinalIgnoreCase) &&
                    StorageConnector.GetContainerNameFromUri(serviceBusMessage.Data.Url).Equals(ByosAudioUploadedEnvironmentVariables.AudioInputContainer, StringComparison.Ordinal))
                {
                    return true;
                }
            }
            catch (JsonSerializationException e)
            {
                logger.LogError($"Exception {e.Message} while parsing message {messageBody} - message will be ignored.");
                return false;
            }

            return false;
        }
    }
}
