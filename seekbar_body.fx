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