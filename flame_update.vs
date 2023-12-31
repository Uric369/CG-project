#version 330 core 
layout (location = 3) in float Type;
layout (location = 4) in vec3 Position;
layout (location = 5) in vec3 Velocity;
layout (location = 6) in float Age;
layout (location = 7) in float alpha;
layout (location = 8) in float size;
layout (location = 9) in float life;
out float Type0;
out vec3 Position0;
out vec3 Velocity0;
out float Age0;
out float Alpha0;
out float Size0;
out float Life0;

void main()
{
	Type0 = Type;
	Position0 = Position;
	Velocity0 = Velocity;
	Age0 = Age;
    Alpha0 = alpha;
    Size0 = size;
    Life0 = life;
}
