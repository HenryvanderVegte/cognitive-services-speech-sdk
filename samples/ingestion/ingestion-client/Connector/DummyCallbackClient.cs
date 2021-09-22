// <copyright file="DummyCallbackClient.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
// </copyright>

namespace Connector
{
    using System;
    using System.Collections.Generic;
    using System.Net.Http;
    using System.Threading.Tasks;
    using Newtonsoft.Json;

    public class DummyCallbackClient
    {
        private readonly HttpClient HttpClient;

        private readonly string ClientId;

        private readonly string ClientSecret;

        private readonly string Scope;

        private readonly Uri TokenEndpointUrl;

        private readonly Uri CallbackBaseUrl;

        public DummyCallbackClient(
            HttpClient httpClient,
            string clientId,
            string clientSecret,
            string scope,
            Uri tokenEndpointUrl,
            Uri callbackBaseUrl)
        {
            this.HttpClient = httpClient;
            this.ClientId = clientId;
            this.ClientSecret = clientSecret;
            this.Scope = scope;
            this.TokenEndpointUrl = tokenEndpointUrl;
            this.CallbackBaseUrl = callbackBaseUrl;
        }

        private Token AccessToken { get; set; }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Performance", "CA1801", Justification = "Implementation is still missing.")]
        public async void PostFileUpdate(string fileName, string status, string resultPath, string message)
        {
            if (this.IsTokenNullOrExpired())
            {
                await RefreshTokenAsync().ConfigureAwait(false);
            }

            var token = this.AccessToken.AccessToken;

            // TODO: Call CallbackBaseUrl with AccessToken, send file info in request body
        }

        private bool IsTokenNullOrExpired()
        {
            if (this.AccessToken == null)
            {
                return true;
            }

            var expiresIn = this.AccessToken.ExpiresIn;

            // TODO: Add logic to check if token is expired
            return false;
        }

        private async Task RefreshTokenAsync()
        {
            var form = new Dictionary<string, string>
                {
                    { "scope", this.Scope },
                    { "client_id", this.ClientId },
                    { "client_secret", this.ClientSecret },
                };

            using var formContent = new FormUrlEncodedContent(form);
            var responseMessage = await this.HttpClient.PostAsync(this.TokenEndpointUrl, formContent).ConfigureAwait(false);
            var jsonContent = await responseMessage.Content.ReadAsStringAsync().ConfigureAwait(false);

            var accessToken = JsonConvert.DeserializeObject<Token>(jsonContent);
            this.AccessToken = accessToken;
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Performance", "CA1812", Justification = "Implementation is still missing.")]
        internal class Token
        {
            public string AccessToken { get; set; }

            public int ExpiresIn { get; set; }
        }
    }
}
