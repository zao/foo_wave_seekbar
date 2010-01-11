Texture1D tex : WAVEFORMDATA;
Texture2D seekTex < string filename = "seekbar.png"; >;

SamplerState sTex
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
};

struct VS_IN
{
	float2 pos : POSITION;
};

struct PS_IN
{
	float4 pos : SV_POSITION;
	float2 tc : TEXCOORD0;
};


float4 backgroundColor : BACKGROUNDCOLOR;
float4 highlightColor  : HIGHLIGHTCOLOR;
float4 selectionColor  : SELECTIONCOLOR;
float4 textColor       : TEXTCOLOR;
float cursorPos        : CURSORPOSITION;
bool cursorVisible     : CURSORVISIBLE;
float seekPos          : SEEKPOSITION;
bool seeking           : SEEKING;
float4 replayGain      : REPLAYGAIN; // album gain, track gain, album peak, track peak
float2 viewportSize    : VIEWPORTSIZE;
bool horizontal        : ORIENTATION;
bool shade_played      : SHADEPLAYED;

PS_IN VS( VS_IN input )
{
	PS_IN output = (PS_IN)0;

	output.pos = float4(input.pos, 0, 1);
	if (horizontal)
		output.tc = float2((input.pos.x + 1.0) / 2.0, input.pos.y);
	else
		output.tc = float2((-input.pos.y + 1.0) / 2.0, input.pos.x);

	return output;
}

float4 bar( float pos, float2 tc, float4 fg, float4 bg, float width, bool show )
{
	float dist = abs(pos - tc.x);
	float4 c = (show && dist < width)
		? lerp(fg, bg, smoothstep(0, width, dist))
		: bg;
	return c;
}

float4 faded_bar( float pos, float2 tc, float4 fg, float4 bg, float width, bool show, float vert_from, float vert_to )
{
	float dist = abs(pos - tc.x);
	float fluff = smoothstep(vert_from, vert_to, abs(tc.y));
	float4 c = show
		? lerp(fg, bg, max(fluff, smoothstep(0, width, dist)))
		: bg;
	return c;
}

float4 played( float pos, float2 tc, float4 fg, float4 bg, float alpha)
{
	float4 c = bg;
	if (pos > tc.x)
		c = lerp(c, fg, saturate(alpha));
	return c;
}

float4 evaluate( float2 tc )
{
	// alpha 1 indicates biased texture
	float4 minmaxrms = tex.Sample(sTex, tc.x);
	minmaxrms.rgb -= 0.5 * minmaxrms.a;
	minmaxrms.rgb *= 1.0 + minmaxrms.a;
	float below = tc.y - minmaxrms.r;
	float above = tc.y - minmaxrms.g;
	float factor = min(abs(below), abs(above));
	bool outside = (below < 0 || above > 0);
	bool inside_rms = abs(tc.y) <= minmaxrms.b;

	float4 wave = outside
		? backgroundColor
		: lerp(backgroundColor, textColor, 7.0 * factor);

	return saturate(wave);
}

float4 PS( PS_IN input ) : SV_Target
{
	float dx, dy;
	if (horizontal)
	{
		dx = 1/viewportSize.x;
		dy = 1/viewportSize.y;
	}
	else
	{
		dx = 1/viewportSize.y;
		dy = 1/viewportSize.x;
	}
	float seekWidth = 2.5 * dx;
	float positionWidth = 2.5 * dx;
	
	float4 c0 = evaluate(input.tc);
	c0 = bar(cursorPos, input.tc, selectionColor, c0, positionWidth, cursorVisible);
	c0 = bar(seekPos,   input.tc, selectionColor, c0, seekWidth,     seeking      );
	if (shade_played)
		c0 = played(cursorPos, input.tc, highlightColor, c0, 0.3);
	return c0;
}

technique10 Render10
{
	pass P0
	{
		SetGeometryShader( 0 );
		SetVertexShader( CompileShader( vs_4_0, VS() ) );
		SetPixelShader( CompileShader( ps_4_0, PS() ) );
	}
}

technique Render9
{
	pass
	{
		VertexShader = compile vs_2_0 VS();
		PixelShader = compile ps_2_0 PS();
	}
}
