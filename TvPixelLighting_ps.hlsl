#include "Common.hlsli"

Texture2D DiffuseSpecularMap : register(t0); // Portal texture
Texture2D TvDiffuseSpecularMap : register(t1); // TV texture

SamplerState TexSampler : register(s0);

float4 main(LightingPixelShaderInput input) : SV_Target
{
    float4 portalTexture = DiffuseSpecularMap.Sample(TexSampler, input.uv);
    float4 tvTexture = TvDiffuseSpecularMap.Sample(TexSampler, input.uv);

    // TV alpha:
    // 0 = screen area
    // 1 = TV frame

    if (tvTexture.a == 0)
    {
        // Convert portal to black & white
        float grey = (portalTexture.r + portalTexture.g + portalTexture.b) / 3.0f;
        return float4(grey, grey, grey, 1.0f);
    }
    else
    {
        // Show TV frame
        return float4(tvTexture.rgb, 1.0f);
    }
}