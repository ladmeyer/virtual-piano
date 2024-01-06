#version 330 core

uniform float iTime;
uniform vec2 iResolution;
uniform mat4 C;

uniform vec3 white_key_pressed[52];
uniform vec3 black_key_pressed[52];

uniform int octave;

uniform vec3 lightPos;
uniform vec3 spotlightPos;

out vec4 fragColor;

float TAU = 6.28318530718;
float RAD(float deg) {
    return deg / 360.0 * TAU;
}

// all below functions taken straight from iq or taken from iq & modified
// draws box centered on p with dimensions b
float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

// draws box centered on p with dimensions b, edges rounded by r
float sdRoundBox( vec3 p, vec3 b, float r )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}

float sdPlane( vec3 p, vec3 n, float h )
{
  // n must be normalized
  return dot(p,n) + h;
}

float sdCappedCylinder(vec3 p, vec3 a, vec3 b, float r)
{
  vec3  ba = b - a;
  vec3  pa = p - a;
  float baba = dot(ba,ba);
  float paba = dot(pa,ba);
  float x = length(pa*baba-ba*paba) - r*baba;
  float y = abs(paba-baba*0.5)-baba*0.5;
  float x2 = x*x;
  float y2 = y*y*baba;
  float d = (max(x,y)<0.0)?-min(x2,y2):(((x>0.0)?x2:0.0)+((y>0.0)?y2:0.0));
  return sign(d)*sqrt(abs(d))/baba;
}

float opRepLim( in float p, in float s, in float min, in float max )
{
    return p - s * clamp(round(p/s), min, max);
}

float opUnion( float d1, float d2 ) { return min(d1,d2); }

float opSubtraction( float d1, float d2 ) { return max(-d1,d2); }

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

// -------------------------------------------------------------------

float white_keys( in vec3 p )
{
    // dimensions: 0.87", 0.75", 6.56" -- 0.05" of space on all sides 

    // repeat keys total of 52 times, every .23 units of space
    vec3 q = vec3( opRepLim(p.x, 0.23, 0.0, 51.0), p.yz );

    float key_body;
    float key_surface;

    // lower key if pressed; otherwise, draw at normal height
    int key_index = int(round(p.x / 0.23));
    if (white_key_pressed[key_index].x == 1.0) {
	    key_body = sdRoundBox( q + vec3(0.0, 0.1, 0.0), vec3(0.087,0.075,0.656)-0.005, 0.01 );    
        key_surface = sdBox( q + vec3(0.0, 0.025, 0.0), vec3(0.1, 0.005, 0.7));
    } else {
        key_body = sdRoundBox( q, vec3(0.087,0.075,0.656)-0.005, 0.01 );    
        key_surface = sdBox( q - vec3(0.0, 0.075, 0.0), vec3(0.1, 0.005, 0.7));
    }

    return opUnion(key_body, key_surface);
}

float black_keys( in vec3 p, in float d ) {
    // d is distance to white keys

    vec3 q = vec3( opRepLim(p.x - 0.115, 0.23, 0.0, 50.0), p.y - 0.185, p.z + 0.3 );
	float k = p.x / 0.23;
    while (k > 7.0) {
        k = k - 7.0;
    }

    // don't draw anything if there shouldn't be a black key here
	if (k > 1.0 && k < 2.0 || k > 4.0 && k < 5.0) {
        return d;
    }

    int key_index = int(floor(p.x / 0.23));
    if (black_key_pressed[key_index].x == 1.0) {
        return sdRoundBox( q + vec3(0.0, 0.1, 0.0), vec3(0.06, 0.075, 0.4), 0.015 );
    }
    return sdRoundBox( q, vec3(0.06, 0.075, 0.4), 0.015 );
}

float piano_body( in vec3 p ) {
    vec3 origin = vec3(5.88, -0.2, 0.0);
    vec3 dimensions = vec3(6.5, 0.1, 1.0);
    float keyboard_base = sdBox(p - origin, dimensions);

    origin = vec3(5.88, -2.0, -2.5);
    dimensions = vec3(6.5, 5.0, 1.5);
    float piano_back = sdBox(p - origin, dimensions);

    origin = vec3(-0.2, -2.7, 0.7);
    dimensions = vec3(0.1, 2.6, 0.1);
    float left_leg = sdBox(p - origin, dimensions);
    origin = vec3(11.96, -2.7, 0.7);
    float right_leg = sdBox(p - origin, dimensions);
    float legs = opUnion(left_leg, right_leg);

    origin = vec3(-0.35, 0.3, -0.2);
    dimensions = vec3(0.1, 0.5, 0.8);
    float left_partition = sdBox(p - origin, dimensions);
    origin = vec3(12.1, 0.3, -0.2);
    float right_partition = sdBox(p - origin, dimensions);
    float partitions = opUnion(left_partition, right_partition);

    float cylinder = sdCappedCylinder(p, vec3(-1.0, 0.8, 0.35), vec3(13.0, 0.8, 0.35), 0.7);
    partitions = opSmoothSubtraction(cylinder, partitions, 0.1);

    origin = vec3(5.88, 1.0, -1.0);
    dimensions = vec3(4.0, 0.06, 0.06);
    float music_stand = sdBox(p - origin, dimensions);

    return opUnion(opUnion(opUnion(opUnion(keyboard_base, piano_back), legs), partitions), music_stand);
}

float piano_floor( in vec3 p ) {
    return sdPlane(p, normalize(vec3(0.0, 1.0, 0.0)), 5.0);
}

float octave_indicator ( in vec3 p ) {
    vec3 offset = vec3(1.6, 0.0, 0.0) * octave;
    if (octave == 7) {
        return sdBox(p - vec3(0.3, -0.15, 0.82) - offset, vec3(0.35, 0.1, 0.1));
    }
    return sdBox(p - vec3(0.83, -0.15, 0.82) - offset, vec3(0.9, 0.1, 0.1));
}

vec2 map (in vec3 p) {
    // result stores distance to nearest thing & an ID identifying it

    // white keys 
    vec2 white_keys = vec2(white_keys(p), 0.0);
    vec2 result = white_keys;

    // black keys
    vec2 black_keys = vec2( black_keys( p, result.x ), 1.0 );
	if( black_keys.x < result.x ) {
        result = black_keys;
    }

    // floor
    vec2 piano_floor = vec2(piano_floor(p), 2.0);
    if ( piano_floor.x < result.x ) {
        result = piano_floor;
    }

    // piano body 
    vec2 piano_body = vec2(piano_body(p), 3.0);
    if ( piano_body.x < result.x ) {
        result = piano_body;
    }

    // octave_indicator
    vec2 octave_indicator = vec2(octave_indicator(p), 4.0);
    if (octave_indicator.x < result.x ) {
        result = octave_indicator;
    }

    return result;
}

// from iq
vec3 calcNormal( in vec3 p )
{
    const float eps = 0.0001; // or some other value
    const vec2 h = vec2(eps,0);
    return normalize( vec3(map(p+h.xyy).x - map(p-h.xyy).x,
                           map(p+h.yxy).x - map(p-h.yxy).x,
                           map(p+h.yyx).x - map(p-h.yyx).x ) );
}

// adapted from iq
vec2 raycast( in vec3 o, in vec3 d )
{
    // o is camera origin 
    // d is camera direction

    // maximum render distance
	float maxd = 25.0;
    // distance marched along ray
    float t = 0.0;
    // object ID
    float m = -1.0;

    for( int i=0; i<64; i++ )
    {
        // get distance & id of nearest thing
	    vec2 h = map(o + d * t);
	    m = h.y;

        // if we've hit the surface of something, break
        if( abs(h.x)<0.001 || t>maxd ) break;

        // otherwise, move along ray
        t += h.x;
    }

    // if the ray hit at least one thing, return a vec2 storing the distance 
    // to the nearest thing and an ID identifying it
    // otherwise, return (-1.0, -1.0)
    return (t<maxd) ? vec2( t, m ) : vec2(-1.0);
}

// for color palettes
vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}

void main() {
    vec3 o = C[3].xyz;
    vec3 d; {
        float theta_over_two = RAD(30.0);
        vec2 d_xy_camera = (gl_FragCoord.xy - (0.5 * iResolution.xy)) * (tan(theta_over_two) / (0.5 * iResolution.y));
        vec3 d_camera = normalize(vec3(d_xy_camera, -1.0));
        d = mat3(C) * d_camera;
    }

    vec2 cast_result = raycast(o, d);

    if (cast_result.y > -1.0) {
        vec3 pos = o + cast_result.x * d;
        vec3 nor = calcNormal(pos);

        vec3 lightDir = normalize(lightPos - pos);
        vec3 spotlightDir = normalize(spotlightPos - pos);
        vec3 lightColor = vec3(1.0);

        vec3 viewDir = normalize(o - pos);
        vec3 reflectDir = reflect(-lightDir, nor);

        vec3 ambient = vec3(0.1);

        float diff = max(dot(nor, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        float spotDiff = max(dot(nor, spotlightDir), 0.0);
        vec3 spotDiffuse = spotDiff * lightColor;

        vec3 color;
        float specularStrength = 0.5;
        if (cast_result.y == 0.0) {
            float color_picker = (iTime * 0.2) + round(pos.x / 0.23) / 20.0;
            color = pal( color_picker, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.10,0.20) );
        } else if (cast_result.y == 1.0) {
            color = vec3(0.1);
        } else if (cast_result.y == 2.0) {
            color = vec3(0.0, 0.2, 0.9);
        } else if (cast_result.y == 3.0) {
            color = pal( pos.y / 12, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.10,0.20) );
        } else if (cast_result.y == 4.0) {
            color = vec3(1.0, 1.0, 0);
        }

        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * lightColor;  

        vec3 result = (ambient + diffuse + specular) * color;

        vec3 spotlight;
        float theta = dot(spotlightDir, normalize(vec3(0.0, 1.0, 0.0)));
        if (theta > 0.95) {
            spotlight = ambient + spotDiffuse * color;
        } else {
            spotlight = vec3(0.0);
        }

        fragColor = vec4(result + spotlight, 1.0);
    } else {
        fragColor = vec4(0.0);
    }
}

