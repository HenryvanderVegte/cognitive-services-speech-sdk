// <copyright file="TranscriptionDefinitionPropertiesLanguageIdentification.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    using System.Collections.Generic;

    public class TranscriptionDefinitionPropertiesLanguageIdentification
    {
        public TranscriptionDefinitionPropertiesLanguageIdentification(IEnumerable<string> candidateLocales)
        {
            this.CandidateLocales = candidateLocales;
        }

        public IEnumerable<string> CandidateLocales { get; set; }
    }
}
