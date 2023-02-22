//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

namespace BatchClient
{
    using System;
    using Newtonsoft.Json;

    public class DisplayWord
    {
        public string DisplayText { get; set; }

        [JsonConverter(typeof(TimeSpanConverter))]
        public TimeSpan Offset { get; set; }

        [JsonConverter(typeof(TimeSpanConverter))]
        public TimeSpan Duration { get; set; }

        public double OffsetInTicks { get; set; }

        public double DurationInTicks { get; set; }
    }
}
