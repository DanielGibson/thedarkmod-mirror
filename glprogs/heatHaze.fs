#version 140
// !!ARBfp1.0 

in vec4 var_tc1;
in vec4 var_tc2;
out vec4 draw_Color;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform vec4 u_scalePotToWindow;
uniform vec4 u_scaleWindowToUnit;

void main() {
	// OPTION ARB_precision_hint_fastest;
	
	// texture 0 is _currentRender
	// texture 1 is a normal map that we will use to deform texture 0
	//
	// env[0] is the 1.0 to _currentRender conversion
	// env[1] is the fragment.position to 0.0 - 1.0 conversion
	
	vec4 localNormal, R0;                                                                               //TEMP	localNormal, R0;
	
	vec4 subOne = vec4(-1, -1, -1, -1);                                                                 //PARAM	subOne = { -1, -1, -1, -1 };
	vec4 scaleTwo = vec4(2, 2, 2, 2);                                                                   //PARAM	scaleTwo = { 2, 2, 2, 2 };
	
	// load the filtered normal map and convert to -1 to 1 range
	localNormal = texture(u_texture1, var_tc1.xy);                                                      //TEX		localNormal, fragment.texcoord[1], texture[1], 2D;
//	localNormal.x = localNormal.a;                                                                      //MOV		localNormal.x, localNormal.a;
	localNormal = (localNormal) * (scaleTwo) + (subOne);                                                //MAD		localNormal, localNormal, scaleTwo, subOne;
	localNormal.z = sqrt(max(0, 1-localNormal.x*localNormal.x-localNormal.y*localNormal.y));
	
	// calculate the screen texcoord in the 0.0 to 1.0 range
	R0 = (gl_FragCoord) * (u_scaleWindowToUnit);                                                        //MUL		R0, fragment.position, program.env[1];
	
	// offset by the scaled localNormal and clamp it to 0.0 - 1.0
	R0 = clamp((localNormal) * (var_tc2) + (R0), 0.0, 1.0);                                             //MAD_SAT	R0, localNormal, fragment.texcoord[2], R0;
	
	// scale by the screen non-power-of-two-adjust
	R0 = (R0) * (u_scalePotToWindow);                                                                   //MUL		R0, R0, program.env[0];
	
	// load the screen render
	draw_Color.xyz = texture(u_texture0, R0.xy).xyz;                                                    //TEX		result.color.xyz, R0, texture[0], 2D;
	
}
