#version 330

// info about our light
uniform vec3      lightPos;                         // position of our light after view matrix was applied
uniform float     ambient = 0.3;		                // ambient factor
uniform vec3		  lightcol = vec3(1.0, 1.0, 1.0);	  // color of the light of our sun
uniform sampler2D shadowMap;                        // our shadow map

// info about our material
uniform float     alpha = 1.0;                      // alpha for our material
uniform sampler2D textureMap;                       // our texture map
uniform vec3      matSpecColor = vec3(1.0, 1.0, 1.0); // specular color of our material
uniform float		  shininess = 30.0;                 // shininess

in vec4           V;                                // position of fragment after modelView matrix was applied
in vec3           Nv;                               // normal vector for our fragment (inc view matrix)
in vec2           T;                                // coordinates for this fragment within our texture map
in vec4           Vs;                               // our shadow map coordinates
out vec4          fragcolor;                        // our output color

// sample our shadow map
float sampleShadowMap(float pZ, vec2 pCoords) {
  float bias = 0.00005;
  float depth = texture(shadowMap, pCoords).x;
  
  if (pZ - bias > depth) {
    return 0.0;
  } else {
    return 1.0;
  };  
}

// check if we're in shadow..
float shadow(vec4 pVs) {
  float factor;
  
  vec3 Proj = pVs.xyz / pVs.w;
  if ((abs(Proj.x)<0.99) && (abs(Proj.y)<0.99) && (abs(Proj.z)<0.99)) {
    // bring it into the range of 0.0 to 1.0 instead of -1.0 to 1.0
    factor = sampleShadowMap(0.5 * Proj.z + 0.5, vec2(0.5 * Proj.x + 0.5, 0.5 * Proj.y + 0.5));
  } else {
    factor = 1.0;
  };

  return factor;
}

void main() {
  // start by getting our color from our texture
  fragcolor = texture(textureMap, T);  
  fragcolor.a = fragcolor.a * alpha;
  if (fragcolor.a < 0.2) {
    discard;
  };
  
  // Get the normalized directional vector between our surface position and our light position
  vec3	L = normalize(lightPos - V.xyz);
  
  // We calculate our ambient color
  vec3  ambientColor = fragcolor.rgb * lightcol * ambient;

  // Check our shadow map
  float shadowFactor = shadow(Vs);
  
  // We calculate our diffuse color, we calculate our dot product between our normal and light
  // direction, note that both were adjusted by our view matrix so they should nicely line up
  float NdotL = max(0.0, dot(Nv, L));
  
  // and calculate our color after lighting is applied
  vec3 diffuseColor = fragcolor.rgb * lightcol * (1.0 - ambient) * NdotL * shadowFactor;	

  // now for our specular lighting
	vec3 specColor = vec3(0.0);
  if ((NdotL != 0.0) && (shininess != 0.0)) {
    // slightly different way to calculate our specular highlight
		vec3	halfVector	= normalize(L - normalize(V.xyz));
		float	nxHalf = max(0.0, dot(Nv, halfVector));
		float	specPower = pow(nxHalf, shininess);
		
    specColor = lightcol * matSpecColor * specPower * shadowFactor;
  };
  
  // and add them all together
  fragcolor = vec4(clamp(ambientColor+diffuseColor+specColor, 0.0, 1.0), fragcolor.a);
}