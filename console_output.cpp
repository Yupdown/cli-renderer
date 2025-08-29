#include <Windows.h>
#include "glm/glm/glm.hpp"
#include "glm/glm/ext.hpp"
#include "OBJ_Loader.h"

std::string windowTitle = "OpenGLExperiment";

constexpr char GREY_LEVEL[] = R"( .:-=+%*@#)";
constexpr int SCREEN_WIDTH = 100;
constexpr int SCREEN_HEIGHT = 100;

glm::vec4 valueBuffer[SCREEN_WIDTH][SCREEN_HEIGHT];
float depthBuffer[SCREEN_WIDTH][SCREEN_HEIGHT];
char textBuffer[(SCREEN_WIDTH + 1) * SCREEN_HEIGHT];

class ShaderProgram
{
public:
	struct Data
	{
		glm::vec4 position;
		glm::vec4 color;
		glm::vec4 normal;

		static Data lerp(const Data& lhs, const Data& rhs, float t)
		{
			Data out;
			out.position = lhs.position + (rhs.position - lhs.position) * t;
			out.color = lhs.color + (rhs.color - lhs.color) * t;
			out.normal = lhs.normal + (rhs.normal - lhs.normal) * t;
			return out;
		}
	};

public:
	glm::mat4 worldTransform;
	glm::mat4 viewTransform;
	glm::mat4 projectTransform;

	glm::vec3 lightDirection;
	glm::vec3 viewPosition;

	glm::vec3 ambientColor;
	glm::vec3 diffuseColor;
	glm::vec3 specularColor;

public:
	Data VertexShader(const Data& in)
	{
		Data out;
		out.position = projectTransform * viewTransform * worldTransform * in.position;
		out.normal = worldTransform * in.normal;
		out.color = in.color;
		return out;
	}

	glm::vec4 FragmentShader(const Data& in)
	{
		// return glm::vec4(glm::vec3(in.color), 1.0f);

		glm::vec3 normal = glm::normalize(in.normal);
		glm::vec3 view_Direction = glm::normalize(viewPosition - glm::vec3(in.position));

		float ambient_Light = 1.0f;
		glm::vec3 ambient = ambientColor * ambient_Light;

		float diffuse_Light = glm::max(dot(normal, lightDirection), 0.0f);
		glm::vec3 diffuse = diffuseColor * diffuse_Light;

		float shiny = 64.0f;
		glm::vec3 reflect_Direction = reflect(-lightDirection, normal);
		float specular_Light = glm::max(0.0f, dot(view_Direction, reflect_Direction));
		specular_Light = pow(specular_Light, shiny);
		glm::vec3 specular = specularColor * specular_Light;

		glm::vec3 color = (ambient + diffuse + specular) * glm::vec3(in.color);
		return glm::vec4(color, 1.0);
	}
};

ShaderProgram* program;

std::vector<float> VERTEX_DATA;
std::vector<float> NORMAL_DATA;
std::vector<unsigned int> INDEX_DATA;

glm::mat4 viewportTransform;

inline float CCW(float ax, float ay, float bx, float by, float cx, float cy)
{
	return (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
}

void DrawScene();
void Timer();
void LoadPolygon(const char* fileName);

float aspect = 1.0f;
float elapsedTime = 0.0f;

int main(int argc, char** argv)
{
	program = new ShaderProgram();

	program->worldTransform = glm::identity<glm::mat4>();
	program->viewTransform = glm::identity<glm::mat4>();
	program->projectTransform = glm::identity<glm::mat4>();

	program->lightDirection = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f));
	program->viewPosition = glm::vec3();
	program->ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
	program->diffuseColor = glm::vec3(0.6, 0.6f, 0.6f);
	program->specularColor = glm::vec3(1.0f, 1.0f, 1.0f);

	viewportTransform = glm::identity<glm::mat4>();
	viewportTransform[0][0] = SCREEN_WIDTH * 0.5f;
	viewportTransform[1][1] = -SCREEN_HEIGHT * 0.5f;
	viewportTransform[3][0] = SCREEN_WIDTH * 0.5f;
	viewportTransform[3][1] = SCREEN_HEIGHT * 0.5f;

	LoadPolygon("cow.obj");

	while (true)
	{
		Timer();
		DrawScene();
	}

	delete program;
	return 0;
}

void Timer()
{

}

void DrawScene()
{
	elapsedTime += 0.05f; // static_cast<float>(clock()) / 1000.0f;

	// clear buffers
	memset(valueBuffer, 0, sizeof(valueBuffer));
	memset(depthBuffer, 0, sizeof(depthBuffer));

	// calculate and append transform matrix
	float aspect = static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT;
	glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.1f, -1.5f)); // glm::sin(elapsedTime) - 2.5f));
	glm::mat4 r = glm::rotate(glm::mat4(1.0f), 0.3f, glm::vec3(1.0f, 0.0f, 0.0f));
	float rt = elapsedTime * 0.1f;
	r = glm::rotate(r, rt, glm::vec3(0.0f, 1.0f, 0.0f));
	r = glm::rotate(r, rt, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f) * 0.1f);

	program->worldTransform = t * r * s;
	program->projectTransform = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f); //glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

	auto funcGetT = [](const glm::vec4& lhs, const glm::vec4& rhs, float y) {
		float t = (y - lhs.y) / (rhs.y - lhs.y);
		return t;
	};

	// add fragment per triangle
	for (int index = 0; index + 2 < INDEX_DATA.size(); index += 3)
	{
		ShaderProgram::Data vertexData[3];

		for (int offset = 0; offset < 3; ++offset)
		{
			ShaderProgram::Data dataIn;
			int vindex = INDEX_DATA[index + offset] * 3;
			dataIn.position = glm::vec4(VERTEX_DATA[vindex], VERTEX_DATA[vindex + 1], VERTEX_DATA[vindex + 2], 1.0f);
			dataIn.color = glm::vec4(1.0f);
			dataIn.normal = glm::vec4(NORMAL_DATA[vindex], NORMAL_DATA[vindex + 1], NORMAL_DATA[vindex + 2], 0.0f);

			vertexData[offset] = program->VertexShader(dataIn);
			glm::vec4 vClip = vertexData[offset].position;

			if (vClip.w != 0.0f)
				vClip /= vClip.w;

			vertexData[offset].position = viewportTransform * vClip;
		}

		if (CCW(vertexData[0].position.x, vertexData[0].position.y,
			vertexData[1].position.x, vertexData[1].position.y,
			vertexData[2].position.x, vertexData[2].position.y) > 0.0f)
			continue;

		if (vertexData[0].position.y > vertexData[1].position.y)
			std::swap(vertexData[0], vertexData[1]);
		if (vertexData[1].position.y > vertexData[2].position.y)
			std::swap(vertexData[1], vertexData[2]);
		if (vertexData[0].position.y > vertexData[1].position.y)
			std::swap(vertexData[0], vertexData[1]);

		float ymin = vertexData[0].position.y;
		float ymax = vertexData[2].position.y;

		int iymin = static_cast<int>(glm::ceil(ymin));
		int iymax = static_cast<int>(glm::ceil(ymax));

		for (int y = iymin; y < iymax; ++y)
		{
			if (y < 0 || y >= SCREEN_HEIGHT)
				continue;
			
			ShaderProgram::Data vfrom;
			ShaderProgram::Data vto;

			float fy = static_cast<float>(y);
			vfrom = ShaderProgram::Data::lerp(vertexData[0], vertexData[2], funcGetT(vertexData[0].position, vertexData[2].position, fy));
			if (fy < vertexData[1].position.y)
				vto = ShaderProgram::Data::lerp(vertexData[0], vertexData[1], funcGetT(vertexData[0].position, vertexData[1].position, fy));
			else
				vto = ShaderProgram::Data::lerp(vertexData[1], vertexData[2], funcGetT(vertexData[1].position, vertexData[2].position, fy));
			if (vfrom.position.x > vto.position.x)
				std::swap(vfrom, vto);

			int ixmin = static_cast<int>(glm::ceil(vfrom.position.x));
			int ixmax = static_cast<int>(glm::ceil(vto.position.x));

			for (int x = ixmin; x < ixmax; ++x)
			{
				if (x < 0 || x >= SCREEN_WIDTH)
					continue;

				ShaderProgram::Data v = ShaderProgram::Data::lerp(vfrom, vto, (x - vfrom.position.x) / (vto.position.x - vfrom.position.x));
				if (depthBuffer[x][y] > 0 && depthBuffer[x][y] <= v.position.z)
					continue;

				valueBuffer[x][y] = program->FragmentShader(v);
				depthBuffer[x][y] = v.position.z;
			}
		}
	}

	// write text from the buffer
	for (int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		for (int x = 0; x < SCREEN_WIDTH; ++x)
		{
			glm::vec4 col = valueBuffer[x][y];
			int value = static_cast<int>(glm::ceil((col.r + col.g + col.b) / 3.0f * sizeof(GREY_LEVEL) - 2));
			textBuffer[x + y * (SCREEN_WIDTH + 1)] = GREY_LEVEL[glm::clamp(value, 0, static_cast<int>(sizeof(GREY_LEVEL) - 2))];
		}
		textBuffer[y * (SCREEN_WIDTH + 1) + SCREEN_WIDTH] = y + 1 < SCREEN_HEIGHT ? '\n' : '\0';
	}

	glm::vec4 offset = glm::vec4(0.8f, 0.5f, 0.0f, 1.0f);
	glm::vec4 textPos = program->projectTransform * t * s * offset;
	if (textPos.w != 0.0f)
		textPos /= textPos.w;
	textPos = viewportTransform * textPos;

	int tx = static_cast<int>(glm::iround(textPos.x));
	int ty = static_cast<int>(glm::iround(textPos.y));

	// const char* src = "<- Name: Yup";
	//strncpy(textBuffer + (tx + ty * (SCREEN_WIDTH + 1)), src, strlen(src));

	// draw onto the screen from the text
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { 0, 0 });
	std::cout << textBuffer;
}

void LoadPolygon(const char* fileName)
{
	using namespace objl;

	Loader loader;
	if (!loader.LoadFile(fileName))
		return;

	for (int i = 0; i < loader.LoadedMeshes.size(); ++i)
	{
		Mesh m = loader.LoadedMeshes[i];

		for (int j = 0; j < m.Vertices.size(); ++j)
		{
			Vector3 p = m.Vertices[j].Position;
			Vector3 n = m.Vertices[j].Normal;

			VERTEX_DATA.push_back(p.X);
			VERTEX_DATA.push_back(p.Y);
			VERTEX_DATA.push_back(p.Z);
			NORMAL_DATA.push_back(n.X);
			NORMAL_DATA.push_back(n.Y);
			NORMAL_DATA.push_back(n.Z);
		}

		for (int j = 0; j < m.Indices.size(); ++j)
		{
			unsigned int u = m.Indices[j];
			INDEX_DATA.push_back(u);
		}
	}
}