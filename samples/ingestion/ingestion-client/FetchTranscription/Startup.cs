// <copyright file="Startup.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

using Microsoft.Azure.Functions.Extensions.DependencyInjection;

[assembly: FunctionsStartup(typeof(FetchTranscription.Startup))]

namespace FetchTranscription
{
    using System;

    using Connector.Database;
    using Connector.Enums;

    using Microsoft.Azure.Functions.Extensions.DependencyInjection;
    using Microsoft.EntityFrameworkCore;
    using Microsoft.Extensions.Azure;
    using Microsoft.Extensions.DependencyInjection;

    public class Startup : FunctionsStartup
    {
        public override void Configure(IFunctionsHostBuilder builder)
        {
            _ = builder ?? throw new ArgumentNullException(nameof(builder));

            if (FetchTranscriptionEnvironmentVariables.UseSqlDatabase)
            {
                builder.Services.AddDbContext<IngestionClientDbContext>(
                  options => SqlServerDbContextOptionsExtensions.UseSqlServer(options, FetchTranscriptionEnvironmentVariables.DatabaseConnectionString));
            }

            builder.Services.AddAzureClients(clientBuilder =>
            {
                clientBuilder.AddServiceBusClient(FetchTranscriptionEnvironmentVariables.StartTranscriptionServiceBusConnectionString)
                    .WithName(ServiceBusClientName.StartTranscriptionServiceBusClient.ToString());
                clientBuilder.AddServiceBusClient(FetchTranscriptionEnvironmentVariables.FetchTranscriptionServiceBusConnectionString)
                    .WithName(ServiceBusClientName.FetchTranscriptionServiceBusClient.ToString());

                if (!string.IsNullOrWhiteSpace(FetchTranscriptionEnvironmentVariables.CompletedServiceBusConnectionString))
                {
                    clientBuilder.AddServiceBusClient(FetchTranscriptionEnvironmentVariables.CompletedServiceBusConnectionString)
                        .WithName(ServiceBusClientName.CompletedTranscriptionServiceBusClient.ToString());
                }
            });
        }
    }
}
