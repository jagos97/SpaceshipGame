#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "main.h"



// An example struct for Game Objects.
// You are encouraged to customize this as you see fit.
int score = 0;
struct GameObject {
	// Struct's constructor deals with the texture.
	// Also sets default position, theta, scale, and transformationMatrix
	GameObject(std::string texturePath, GLenum textureInterpolation) :
		texture(texturePath, textureInterpolation),
		position(0.0f, 0.0f, 0.0f, 1.f),
		theta(0),
		scale(1),
		transformationMatrix(1.0f), // This constructor sets it as the identity matrix
		active(true)
	{}

	bool active = true;
	CPU_Geometry cgeom;
	GPU_Geometry ggeom;
	Texture texture;

	glm::vec4 position;
	glm::vec4 defaultPosition;
	glm::mat4 defaultTransformationMatrix;
	std::vector<std::shared_ptr<GameObject>> children;
	std::shared_ptr<GameObject> parent;
	std::vector<glm::mat4> transformations;
	float theta; // Object's rotation
	// Alternatively, you could represent rotation via a normalized heading vec:
	// glm::vec3 heading;
	float scale; // Or, alternatively, a glm::vec2 scale;
	glm::mat4 transformationMatrix;
};

const float PI = 3.14159265359;

glm::mat4 MakeRotationMatrix(float theta) {
	glm::mat4 rotation(
		cos(theta), -sin(theta), 0.f, 0.f,
		sin(theta), cos(theta), 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	);
	return rotation;
}

glm::mat4 MakeTranslationMatrix(float distance, float theta) {
	glm::mat4 translation(
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		cos(theta) *distance, sin(theta) *distance, 0.f, 1.f
	);
	return translation;
}

glm::mat4 MakeTranslationMatrixXY(float x, float y) {
	glm::mat4 translation(
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		x, y, 0.f, 1.f
	);
	return translation;
}

glm::mat4 MakeScaleMatrix(float scale) {
	glm::mat4 scaling(
		scale, 0.f, 0.f, 0.f,
		0.f, scale, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	);
	return scaling;
}

glm::mat4 MakeScaleMatrixXY(float x, float y) {
	glm::mat4 scaling(
		x, 0.f, 0.f, 0.f,
		0.f, y, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	);
	return scaling;
}



glm::mat4 Reset(glm::vec4 pos, float direction) {
	glm::mat4 translation(
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		direction * pos.x, direction * pos.y, 0.f, 1.f
	);
	return translation;
}

glm::mat4 MakeChildrenMatrix(glm::vec4 parentPos, float theta, glm::vec4 childPos, int numOfChildren) {
	glm::mat4 matrix = Reset(childPos, 1) * MakeRotationMatrix(theta) * Reset(childPos, -1);
	float x = parentPos.x - childPos.x + cos(theta + PI)*numOfChildren * 0.15f;
	float y = parentPos.y - childPos.y + sin(theta + PI) * numOfChildren * 0.15f;
	matrix = MakeTranslationMatrixXY(x, y) * matrix;
	return matrix;
}


bool Close(glm::vec4 pos1, glm::vec4 pos2) {
	float x = pos2.x - pos1.x;
	float y = pos2.y - pos1.y;
	return sqrt(x * x + y * y) < 0.1f;
}

bool Goleft(float theta, float angle) {
	float distanceNeg;
	float distancePos;
	if (angle > theta) {
		distancePos = angle - theta;
		distanceNeg = theta - (angle - 2 * PI);
	}
	else {
		distanceNeg = theta - angle;
		distancePos = (angle + 2 * PI) - theta;
	}

	if (distancePos <= distanceNeg) return true;
	else return false;
}

// EXAMPLE CALLBACKS
class MyCallbacks : public CallbackInterface {

public:
	MyCallbacks(ShaderProgram& shader, int width, int height) :
		screenDim(width,height),
		shader(shader) {
		xDiv = width / 2.0f;
		yDiv = height / 2.0f;
	}

	virtual void keyCallback(int key, int scancode, int action, int mods) {
		if(action == GLFW_PRESS) {
			if (key == GLFW_KEY_R) {
				shader.recompile();
			}
			else if (key == GLFW_KEY_W) {
				moveForward = true;
			}
			else if (key == GLFW_KEY_S) {
				moveBack = true;
			}
			else if (key == GLFW_KEY_SPACE) {
				reset = true;
			}
		}
		else if (action == GLFW_RELEASE) {
			if (key == GLFW_KEY_W) {
			moveForward = false;
			}
			else if (key == GLFW_KEY_S) {
			moveBack = false;
			}
		}
	}

	virtual void mouseButtonCallback(int button, int action, int mods) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == GLFW_PRESS || action == GLFW_REPEAT) {
				GLmouse();
				leftPressed = true;
			}
			else if (action == GLFW_RELEASE) {
				leftPressed = false;
			}
		}
	}

	void GLmouse() {
		clickPos = glm::vec2(mousePos.x / xDiv - 1, mousePos.y / -yDiv + 1);
	}

	virtual void cursorPosCallback(double xpos, double ypos) {
		mousePos.x = xpos;
		mousePos.y = ypos;

	}

	glm::vec2 GetClickPosition() {
		return clickPos;
	}

	bool GetLeftPressed() {
		return leftPressed;
	}
	bool GetMoveForward() {
		return moveForward;
	}
	bool GetMoveBack() {
		return moveBack;
	}
	bool GetReset() {
		return reset;
	}
	void ResetDone() {
		reset = false;
	}


private:
	float xDiv;
	float yDiv;
	glm::vec2 screenDim;
	glm::vec2 mousePos = glm::vec2(1.0f);
	glm::vec2 clickPos;
	ShaderProgram& shader;
	bool leftPressed = false;
	bool moveForward = false;
	bool moveBack = false;
	bool reset = false;
};


CPU_Geometry shipGeom(float width, float height) {
	float halfWidth = width / 2.0f;
	float halfHeight = height / 2.0f;
	CPU_Geometry retGeom;
	
	retGeom.verts.push_back(glm::vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(-1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, 1.f, 0.f));
	

	// texture coordinates
	retGeom.texCoords.push_back(glm::vec2(0.f, 1.f));
	retGeom.texCoords.push_back(glm::vec2(0.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(0.f, 1.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 1.f));
	return retGeom;
}

CPU_Geometry DiamondGeom(float width, float height) {
	float halfWidth = width / 2.0f;
	float halfHeight = height / 2.0f;

	CPU_Geometry retGeom;
	
	retGeom.verts.push_back(glm::vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(-1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(glm::vec3(1.f, 1.f, 0.f));

	// texture coordinates
	retGeom.texCoords.push_back(glm::vec2(0.f, 1.f));
	retGeom.texCoords.push_back(glm::vec2(0.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(0.f, 1.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 0.f));
	retGeom.texCoords.push_back(glm::vec2(1.f, 1.f));
	return retGeom;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
///////////////													//////////////////////////////
///////////////				Movement Variables					//////////////////////////////
///////////////													//////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
float movingDistance = 1.0f / 2000.0f;
float rotationDistance = PI / 1500.0f;



int main() {
	Log::debug("Starting main");

	int screenWidth = 800;
	int	screenHeight = 800;

	// WINDOW
	glfwInit();
	Window window(screenWidth, screenHeight, "CPSC 453"); // can set callbacks at construction if desired


	GLDebug::enable();

	// SHADERS
	ShaderProgram shader("shaders/test.vert", "shaders/test.frag");

	// CALLBACKS
	auto callbacks = std::make_shared<MyCallbacks>(shader, screenWidth, screenHeight);
	window.setCallbacks(callbacks); // can also update callbacks to new ones


	// GL_NEAREST looks a bit better for low-res pixel art than GL_LINEAR.
	// But for most other cases, you'd want GL_LINEAR interpolation.
	std::shared_ptr<GameObject> ship = std::make_shared<GameObject>("textures/ship.png", GL_NEAREST);


	ship->defaultTransformationMatrix = MakeScaleMatrixXY(0.15f, 0.10);
	ship->defaultPosition = glm::vec4(0.f);
	ship->cgeom = shipGeom(0.15f, 0.12f);
	ship->ggeom.setVerts(ship->cgeom.verts);
	ship->ggeom.setTexCoords(ship->cgeom.texCoords);
	ship->theta = PI / 2.0f;
	ship->transformationMatrix = ship->defaultTransformationMatrix;


	std::vector<std::shared_ptr<GameObject>> diamonds;

	for (int i = 0; i < 3; i++) {
		std::shared_ptr<GameObject> d = std::make_shared<GameObject>("textures/diamond.png", GL_LINEAR);
		d->cgeom = DiamondGeom(0.2f, 0.2f);
		d->ggeom.setVerts(d->cgeom.verts);
		d->ggeom.setTexCoords(d->cgeom.texCoords);
		diamonds.push_back(d);
	}

	diamonds.at(0)->defaultTransformationMatrix = MakeTranslationMatrix(1, PI / 4) * MakeScaleMatrix(0.10f);
	diamonds.at(1)->defaultTransformationMatrix = MakeTranslationMatrix(1, 3 * PI / 4) * MakeScaleMatrix(0.10f);
	diamonds.at(2)->defaultTransformationMatrix = MakeTranslationMatrix(0.8, 3 * PI / 2) * MakeScaleMatrix(0.10f);
	diamonds.at(0)->position = glm::vec4(glm::vec3(cos(PI/4), sin(PI/4), 0), 0.f);
	diamonds.at(1)->position = glm::vec4(glm::vec3(cos(3*PI/4), sin(3 * PI / 4), 0), 0.f);
	diamonds.at(2)->position = glm::vec4(glm::vec3(0, -0.8f, 0), 0.f);

	for (int i = 0; i < diamonds.size(); i++) {
		diamonds.at(i)->defaultPosition = diamonds.at(i)->position;
		diamonds.at(i)->transformationMatrix = diamonds.at(i)->defaultTransformationMatrix;
	}

	std::vector<std::shared_ptr<GameObject>> fires;
	for (int i = 0; i < 3; i++) {
		std::shared_ptr<GameObject> f = std::make_shared<GameObject>("textures/fire.png", GL_LINEAR);
		f->cgeom = DiamondGeom(0.2f, 0.2f);
		f->ggeom.setVerts(f->cgeom.verts);
		f->ggeom.setTexCoords(f->cgeom.texCoords);
		f->position = diamonds.at(i)->position;
		f->position.y += 0.2f;
		f->theta = PI / 2;
		f->transformationMatrix = MakeScaleMatrixXY(0.3f, 0.4f);
		f->transformationMatrix = diamonds.at(i)->transformationMatrix * f->transformationMatrix;
		f->transformationMatrix = MakeTranslationMatrixXY(0, 0.2f) * f->transformationMatrix;
		f->defaultTransformationMatrix = f->transformationMatrix;
		f->active = true;
		fires.push_back(f);
	}


	// RENDER LOOP
	while (!window.shouldClose()) {
		glfwPollEvents();

		shader.use();

		GLint myLoc = glGetUniformLocation(shader.GetProgram(), "transformation");

		//Moving forward
		if (callbacks ->GetMoveForward()){
			ship->transformationMatrix = MakeTranslationMatrix(movingDistance, ship->theta) * ship->transformationMatrix;
			ship->position.x = ship->position.x + movingDistance * cos(ship->theta);
			ship->position.y = ship->position.y + movingDistance * sin(ship->theta);

			//moving the children forward and saving the transformations for the fires for after
			for (int i = 0; i < ship->children.size(); i++) {
				glm::mat4 matrix = MakeTranslationMatrix(movingDistance, ship->theta);
				ship->children.at(i)->position.x += movingDistance * cos(ship->theta);
				ship->children.at(i)->position.y += movingDistance * sin(ship->theta);
				ship->children.at(i)->transformationMatrix = matrix * ship->children.at(i)->transformationMatrix;
				ship->children.at(i)->transformations.push_back(matrix);
			}

			//Hitboxes for diamonds
			for (int i = 0; i < diamonds.size(); i++) {
				if (diamonds.at(i)->active) {

					//succesful hit on a diamond while moving forward and its active
					if (Close(diamonds.at(i)->position, ship->position)) {
						diamonds.at(i)->active = false;
						score ++ ;
						ship->transformationMatrix = Reset(ship->position, 1.f) * MakeScaleMatrix(1.1f) * Reset(ship->position, -1.f) * ship->transformationMatrix;
						ship->children.push_back(diamonds.at(i));
						diamonds.at(i)->parent = ship;
						diamonds.at(i)->theta = ship->theta;
						glm::mat4 matrix = Reset(diamonds.at(i)->position, 1.f) * MakeScaleMatrix(0.5f) * Reset(diamonds.at(i)->position, -1.f);
						glm::mat4 matrix2 = MakeChildrenMatrix(ship->position, ship->theta, diamonds.at(i)->position, ship->children.size());
						diamonds.at(i)->transformationMatrix = matrix2 *matrix * diamonds.at(i)->transformationMatrix;
						diamonds.at(i)->transformations.push_back(matrix);
						diamonds.at(i)->transformations.push_back(matrix2);
						diamonds.at(i)->position.x = ship->position.x + cos(ship->theta + PI) * ship->children.size() * 0.15f;
						diamonds.at(i)->position.y = ship->position.y + sin(ship->theta + PI) * ship->children.size() * 0.15f;
						fires.at(i)->active = false;		//disabling the fire so it doesn't hit the ship
					}
				}
			}
		}

		//moving backwards
		if (callbacks->GetMoveBack()) {
			ship->transformationMatrix = MakeTranslationMatrix(-movingDistance, ship->theta) * ship->transformationMatrix;
			ship->position.x = ship->position.x - movingDistance * cos(ship->theta);
			ship->position.y = ship->position.y - movingDistance * sin(ship->theta);

			//Moving children and saving the transformations for the fires for after
			for (int i = 0; i < ship->children.size(); i++) {
				glm::mat4 matrix = MakeTranslationMatrix(-movingDistance, ship->theta);
				ship->children.at(i)->position.x += -movingDistance * cos(ship->theta);
				ship->children.at(i)->position.y += -movingDistance * sin(ship->theta);
				ship->children.at(i)->transformationMatrix = matrix * ship->children.at(i)->transformationMatrix;
				ship->children.at(i)->transformations.push_back(matrix);
			}

			//Hitboxes for diamonds
			for (int i = 0; i < diamonds.size(); i++) {
				if (diamonds.at(i)->active) {
					if (Close(diamonds.at(i)->position, ship->position)) {
						diamonds.at(i)->active = false;
						score ++;
						ship->transformationMatrix = Reset(ship->position, 1.f) * MakeScaleMatrix(1.1f) * Reset(ship->position, -1.f) * ship->transformationMatrix;
						ship->children.push_back(diamonds.at(i));
						diamonds.at(i)->theta = ship->theta;
						diamonds.at(i)->parent = ship;
						glm::mat4 matrix = Reset(diamonds.at(i)->position, 1.f) * MakeScaleMatrix(0.5f) * Reset(diamonds.at(i)->position, -1.f);
						glm::mat4 matrix2 = MakeChildrenMatrix(ship->position, ship->theta, diamonds.at(i)->position, ship->children.size());
						diamonds.at(i)->transformationMatrix = matrix2 * matrix * diamonds.at(i)->transformationMatrix;
						diamonds.at(i)->transformations.push_back(matrix);
						diamonds.at(i)->transformations.push_back(matrix2);
						diamonds.at(i)->position.x += ship->position.x - diamonds.at(i)->position.x + cos(ship->theta + PI) * ship->children.size() * 0.15f;
						diamonds.at(i)->position.y += ship->position.y - diamonds.at(i)->position.y + sin(ship->theta + PI) * ship->children.size() * 0.15f;
						fires.at(i)->active = false;
					}
				}
			}
		}

		//turning
		if (callbacks->GetLeftPressed()) {
			callbacks-> GLmouse();
			float x = callbacks->GetClickPosition().x;
			float y = callbacks->GetClickPosition().y;
			x = x - ship->position.x;
			y = y - ship->position.y;
			float angle = atan2(y,x);

			//if the turning distance is more than how much we rotate by then rotate else don't do anything
			if (abs(ship->theta - angle) > rotationDistance) {
				if (angle < 0) {
					angle += 2 * PI;
				}

				//If going clockwise is more eficient
				if (!Goleft(ship->theta,angle)) {
					ship->transformationMatrix = Reset(ship->position, 1.f) * MakeRotationMatrix(rotationDistance) * Reset(ship->position, -1.f) * ship->transformationMatrix;
					ship->theta -= rotationDistance;

					//rotate children and fires of the children
					for (int i = 0; i < ship->children.size(); i++) {
						glm::mat4 matrix = Reset(ship->position, 1.f) * MakeRotationMatrix(rotationDistance) * Reset(ship->position, -1.f);
						ship->children.at(i)->transformationMatrix = matrix * ship->children.at(i)->transformationMatrix;
						ship->children.at(i)->theta = ship->theta;
						ship->children.at(i)->position.x = ship->position.x + cos(ship->theta + PI) * (i+1) * 0.15f;
						ship->children.at(i)->position.y = ship->position.y + sin(ship->theta + PI) * (i+1) * 0.15f;
						ship->children.at(i)->transformations.push_back(matrix);
					}
				}

				//if going counterclockwise is more efficient
				else {
					ship->transformationMatrix = Reset(ship->position, 1.f) * MakeRotationMatrix(-rotationDistance) * Reset(ship->position, -1.f) * ship->transformationMatrix;
					ship->theta += rotationDistance;

					//rotate children and fires of the children
					for (int i = 0; i < ship->children.size(); i++) {
						glm::mat4 matrix = Reset(ship->position, 1.f) * MakeRotationMatrix(-rotationDistance) * Reset(ship->position, -1.f);
						ship->children.at(i)->transformationMatrix = matrix * ship->children.at(i)->transformationMatrix;
						ship->children.at(i)->theta = ship->theta;
						ship->children.at(i)->position.x = ship->position.x + cos(ship->theta + PI) * (i + 1) * 0.15f;
						ship->children.at(i)->position.y = ship->position.y + sin(ship->theta + PI) * (i + 1) * 0.15f;
						ship->children.at(i)->transformations.push_back(matrix);
					}
				}
				//making sure angle stays between 0 and 2PI
				if (ship->theta >= 2 * PI) ship->theta -= 2 * PI;
				if (ship->theta <= 0) ship->theta += 2 * PI;
			}
		}

		//resest game if player pressed space
		if (callbacks->GetReset()) {
			ship->transformationMatrix = ship->defaultTransformationMatrix;
			ship->theta = PI / 2;
			ship->position = ship->defaultPosition;
			ship->children.clear();
			for (int i = 0; i < diamonds.size(); i++) {
				std::cout << diamonds.at(i)->parent << std::endl;
				diamonds.at(i)->transformationMatrix = diamonds.at(i)->defaultTransformationMatrix;
				diamonds.at(i)->active = true;
				diamonds.at(i)->parent = nullptr;
				diamonds.at(i)->position = diamonds.at(i)->defaultPosition;
				diamonds.at(i)->theta = 0;
			}
			for (int i = 0; i < fires.size(); i++) {
				fires.at(i)->transformationMatrix = fires.at(i)->defaultTransformationMatrix;
				fires.at(i)->active = true;
				fires.at(i)->position = fires.at(i)->defaultPosition;
				fires.at(i)->theta = PI/2;
			}
			score = 0;
			callbacks->ResetDone();
		}

		glUniformMatrix4fv(myLoc,
			1,
			false,
			&ship->transformationMatrix[0][0]
		);

		ship->ggeom.bind();
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		ship->texture.bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		ship->texture.unbind();

		for (int i = 0; i < diamonds.size(); i++) {
			if (score >= diamonds.size()) {
				diamonds.at(i)->transformationMatrix = Reset(diamonds.at(i)->position, 1.f) * MakeRotationMatrix(rotationDistance) * Reset(diamonds.at(i)->position, -1.f) * diamonds.at(i)->transformationMatrix;
			}
				glUniformMatrix4fv(myLoc,
					1,
					false,
					&diamonds.at(i)->transformationMatrix[0][0]
				);
				diamonds.at(i)->ggeom.bind();
				diamonds.at(i)->texture.bind();
				glDrawArrays(GL_TRIANGLES, 0, 6);
				diamonds.at(i)->texture.unbind();
			
		}
		for (int i = 0; i < fires.size(); i++) {

			fires.at(i)->transformationMatrix = Reset(diamonds.at(i)->position, 1) * MakeRotationMatrix(rotationDistance) * Reset(diamonds.at(i)->position, -1) * fires.at(i)->transformationMatrix;
			fires.at(i)->theta += rotationDistance;
			fires.at(i)->position.x = diamonds.at(i)->position.x - 0.2f * cos(fires.at(i)->theta);
			fires.at(i)->position.y = 0.2f * sin(fires.at(i)->theta) + diamonds.at(i)->position.y;
			for (int j = 0; j < diamonds.at(i)->transformations.size(); j++) {
				fires.at(i)->transformationMatrix = diamonds.at(i)->transformations.at(j) * fires.at(i)->transformationMatrix;
			}
			diamonds.at(i)->transformations.clear();
			glUniformMatrix4fv(myLoc,
				1,
				false,
				&fires.at(i)->transformationMatrix[0][0]
			);
			fires.at(i)->ggeom.bind();
			fires.at(i)->texture.bind();
			glDrawArrays(GL_TRIANGLES, 0, 6);
			fires.at(i)->texture.unbind();
		}


		//hitbox for fire
		for (int i = 0; i < fires.size(); i++) {
			if (fires.at(i)->active) {
				//reset game if fire was hit while it was active ie parent of fire not child of ship
				if (Close(fires.at(i)->position, ship->position)) {
					ship->transformationMatrix = ship->defaultTransformationMatrix;
					ship->theta = PI / 2;
					ship->position = ship->defaultPosition;
					ship->children.clear();
					for (int i = 0; i < diamonds.size(); i++) {
						std::cout << diamonds.at(i)->parent << std::endl;
						diamonds.at(i)->transformationMatrix = diamonds.at(i)->defaultTransformationMatrix;
						diamonds.at(i)->active = true;
						diamonds.at(i)->parent = nullptr;
						diamonds.at(i)->position = diamonds.at(i)->defaultPosition;
						diamonds.at(i)->theta = 0;
					}
					for (int i = 0; i < fires.size(); i++) {
						fires.at(i)->transformationMatrix = fires.at(i)->defaultTransformationMatrix;
						fires.at(i)->active = true;
						fires.at(i)->position = fires.at(i)->defaultPosition;
						fires.at(i)->theta = PI/2;
					}
					score = 0;
					callbacks->ResetDone();
				}
			}
		}

		glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui
		

		// Starting the new ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// Putting the text-containing window in the top-left of the screen.
		ImGui::SetNextWindowPos(ImVec2(5, 5));

		// Setting flags
		ImGuiWindowFlags textWindowFlags =
			ImGuiWindowFlags_NoMove |				// text "window" should not move
			ImGuiWindowFlags_NoResize |				// should not resize
			ImGuiWindowFlags_NoCollapse |			// should not collapse
			ImGuiWindowFlags_NoSavedSettings |		// don't want saved settings mucking things up
			ImGuiWindowFlags_AlwaysAutoResize |		// window should auto-resize to fit the text
			ImGuiWindowFlags_NoBackground |			// window should be transparent; only the text should be visible
			ImGuiWindowFlags_NoDecoration |			// no decoration; only the text should be visible
			ImGuiWindowFlags_NoTitleBar;			// no title; only the text should be visible

		// Begin a new window with these flags. (bool *)0 is the "default" value for its argument.
		ImGui::Begin("scoreText", (bool *)0, textWindowFlags);

		// Scale up text a little, and set its value
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("Score: %d", score ); // Second parameter gets passed into "%d"
		if (score >= diamonds.size()) {
			ImGui::SetWindowFontScale(8.0f);
			ImGui::Text("\n\n  YOU WIN!!!");
			ImGui::SetWindowFontScale(4.0f);
			ImGui::Text("   Press Space to reset");
		}

		// End the window.
		ImGui::End();

		ImGui::Render();	// Render the ImGui window
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Some middleware thing

		window.swapBuffers();
	}
	// ImGui cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
