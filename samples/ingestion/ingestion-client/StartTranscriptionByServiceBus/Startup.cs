// <copyright file="Startup.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace StartTranscriptionByServiceBus
{
    using System;

    using Connector.Enums;

    using Microsoft.Azure.Functions.Extensions.DependencyInjection;
    using Microsoft.Extensions.Azure;
    using StartTranscriptionByTimer;

    public class Startup : FunctionsStartup
    {
        public override void Configure(IFunctionsHostBuilder builder)
        {
            _ = builder ?? throw new ArgumentNullException(nameof(builder));
            builder.Services.AddAzureClients(clientBuilder =>
            {
                clientBuilder.AddServiceBusClient(StartTranscriptionEnvironmentVariables.StartTranscriptionServiceBusConnectionString)
                    .WithName(ServiceBusClientName.StartTranscriptionServiceBusClient.ToString());
                clientBuilder.AddServiceBusClient(StartTranscriptionEnvironmentVariables.FetchTranscriptionServiceBusConnectionString)
                    .WithName(ServiceBusClientName.FetchTranscriptionServiceBusClient.ToString());
            });
        }
    }
}