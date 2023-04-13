// <copyright file="TranscriptionDefinitionProperties.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    public class TranscriptionDefinitionProperties
    {
        public TranscriptionDefinitionProperties()
        {
        }

        public string ProfanityFilterMode { get; set; }

        public string PunctuationMode { get; set; }

        public string DiarizationEnabled { get; set; }

        public string WordLevelTimestampsEnabled { get; set; }

        public TranscriptionDefinitionPropertiesLanguageIdentification LanguageIdentification { get; set; }
    }
}
