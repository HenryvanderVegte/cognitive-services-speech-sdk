// <copyright file="TranscriptionDefinition.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    using System;
    using System.Collections.Generic;
    using System.Linq;

    public sealed class TranscriptionDefinition
    {
        private TranscriptionDefinition(
            string name,
            string description,
            IEnumerable<string> candidateLocales,
            IEnumerable<string> contentUrls,
            TranscriptionDefinitionProperties properties,
            TranscriptionDefinitionCustomProperties customProperties,
            ModelIdentity model)
        {
            _ = properties ?? throw new ArgumentNullException(nameof(properties));

            this.DisplayName = name;
            this.Description = description;
            this.ContentUrls = contentUrls;
            this.Model = model;
            this.Locale = candidateLocales.First();

            properties.LanguageIdentification = new TranscriptionDefinitionPropertiesLanguageIdentification(candidateLocales);
            this.Properties = properties;

            this.CustomProperties = customProperties;
        }

        public string DisplayName { get; set; }

        public string Description { get; set; }

        public IEnumerable<string> ContentUrls { get; }

        public string Locale { get; set; }

        public ModelIdentity Model { get; set; }

        public TranscriptionDefinitionProperties Properties { get; }

        public TranscriptionDefinitionCustomProperties CustomProperties { get; }

        public static TranscriptionDefinition Create(
            string name,
            string description,
            IEnumerable<string> candidateLocales,
            string contentUrl,
            TranscriptionDefinitionProperties properties,
            TranscriptionDefinitionCustomProperties customProperties,
            ModelIdentity model)
        {
            return new TranscriptionDefinition(name, description, candidateLocales, new[] { contentUrl }.ToList(), properties, customProperties, model);
        }

        public static TranscriptionDefinition Create(
            string name,
            string description,
            IEnumerable<string> candidateLocales,
            IEnumerable<string> contentUrls,
            TranscriptionDefinitionProperties properties,
            TranscriptionDefinitionCustomProperties customProperties,
            ModelIdentity model)
        {
            return new TranscriptionDefinition(name, description, candidateLocales, contentUrls, properties, customProperties, model);
        }
    }
}
