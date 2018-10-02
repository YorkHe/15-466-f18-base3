#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "texture_program.hpp"
#include "depth_program.hpp"

#include "Spider.h"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <cstring>

#include <iostream>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("maze.pnct"));
});

Load< GLuint > meshes_for_texture_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(texture_program->program));
});

Load< GLuint > meshes_for_depth_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(depth_program->program));
});

//used for fullscreen passes:
Load< GLuint > empty_vao(LoadTagDefault, [](){
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindVertexArray(0);
	return new GLuint(vao);
});

Load< GLuint > blur_program(LoadTagDefault, [](){
	GLuint program = compile_program(
		//this draws a triangle that covers the entire screen:
		"#version 330\n"
		"void main() {\n"
		"	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
		"}\n"
		,
		//NOTE on reading screen texture:
		//texelFetch() gives direct pixel access with integer coordinates, but accessing out-of-bounds pixel is undefined:
		//	vec4 color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
		//texture() requires using [0,1] coordinates, but handles out-of-bounds more gracefully (using wrap settings of underlying texture):
		//	vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex,0));

		"#version 330\n"
		"uniform sampler2D tex;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec2 at = (gl_FragCoord.xy - 0.5 * textureSize(tex, 0)) / textureSize(tex, 0).y;\n"
		//make blur amount more near the edges and less in the middle:
		"	float amt = (0.01 * textureSize(tex,0).y) * max(0.0,(length(at) - 0.3)/0.2);\n"
		//pick a vector to move in for blur using function inspired by:
		//https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
		"	vec2 ofs = amt * normalize(vec2(\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(12.9898,78.233))),\n"
		"		fract(dot(gl_FragCoord.xy ,vec2(96.3869,-27.5796)))\n"
		"	));\n"
		//do a four-pixel average to blur:
		"	vec4 blur =\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.x,ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.y,ofs.x)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.x,-ofs.y)) / textureSize(tex, 0))\n"
		"		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.y,-ofs.x)) / textureSize(tex, 0))\n"
		"	;\n"
		"	fragColor = vec4(blur.rgb, 1.0);\n" //blur;\n"
		"}\n"
	);

	glUseProgram(program);

	glUniform1i(glGetUniformLocation(program, "tex"), 0);

	glUseProgram(0);

	return new GLuint(program);
});


GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

Load< GLuint > wood_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wood.png")));
});

Load< GLuint > marble_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/marble.png")));
});

Load< GLuint > stone_bump_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/stone_blocks_bump.png")));
});

Load < GLuint > stone_spec_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/stone_blocks_spec.png")));
});

Load < GLuint > spider_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/spider.png")));
});

Load< GLuint > white_tex(LoadTagDefault, [](){
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(white));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return new GLuint(tex);
});


Scene::Transform *camera_parent_transform = nullptr;
Scene::Camera *camera = nullptr;
Scene::Transform *spot_parent_transform = nullptr;
Scene::Lamp *spot = nullptr;
std::vector<Scene::Lamp*> spot_lights;
std::vector<Spider*> spiders;

Scene::Transform* statue = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;

	//pre-build some program info (material) blocks to assign to each object:
	Scene::Object::ProgramInfo texture_program_info;
	texture_program_info.program = texture_program->program;
	texture_program_info.vao = *meshes_for_texture_program;
	texture_program_info.mvp_mat4  = texture_program->object_to_clip_mat4;
	texture_program_info.mv_mat4x3 = texture_program->object_to_light_mat4x3;
	texture_program_info.itmv_mat3 = texture_program->normal_to_light_mat3;

	Scene::Object::ProgramInfo depth_program_info;
	depth_program_info.program = depth_program->program;
	depth_program_info.vao = *meshes_for_depth_program;
	depth_program_info.mvp_mat4  = depth_program->object_to_clip_mat4;


	//load transform hierarchy:
	ret->load(data_path("maze.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Object *obj = s.new_object(t);
		std::cerr << "Loading: " << m << std::endl;

		obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
		if (t->name == "Wall") {
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *stone_spec_tex;
		} else if (std::strstr(t->name.c_str(), "Spider") != NULL) {
			spiders.push_back(new Spider(t));
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *spider_tex;
		}else if (t->name == "Suzanne") {
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *marble_tex;
			statue = t;
		}
		else{
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *white_tex;
		}

		obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;

		MeshBuffer::Mesh const &mesh = meshes->lookup(m);
		obj->programs[Scene::Object::ProgramTypeDefault].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeDefault].count = mesh.count;

		obj->programs[Scene::Object::ProgramTypeShadow].start = mesh.start;
		obj->programs[Scene::Object::ProgramTypeShadow].count = mesh.count;
	});

	std::cerr << "Finish loading" << std::endl;

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

    std::cerr << "Finish Camera" << std::endl;
	//look up the spotlight:
	for (Scene::Lamp *l = ret->first_lamp; l != nullptr; l = l->alloc_next) {
		if (l->transform->name == "Lamp") {
			spot = l;
			spot_lights.push_back(l);
		}
		if (std::strstr(l->transform->name.c_str(), "SpotLight") != NULL) {
		    spot_lights.push_back(l);
		}
	}
	if (!spot) throw std::runtime_error("No 'Spot' spotlight in scene.");

	std::cerr << "Finish Light" << std::endl;
	return ret;
});

GameMode::GameMode() {
	std::cerr << "START" << std::endl;

	this->walk_mesh = new WalkMesh(data_path("maze.w"));
	walk_point = this->walk_mesh->start(glm::vec3(-20.0f, -20.0f, 1.5f));

	auto position = walk_mesh->world_point(walk_point);
	std::cerr << "WalkPoint" << walk_point.triangle.x << "," << walk_point.triangle.y << "," << walk_point.triangle.z << std::endl;
	std::cerr << "position" << position.x << "," << position.y << "," << position.z << std::endl;
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (game_over || win) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		return false;
	}

	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		}

		if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		}

		if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		}

		if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
			mouse_captured = !mouse_captured;
			if(mouse_captured) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
			} else {
				SDL_SetRelativeMouseMode(SDL_FALSE);
			}
		}
	}

	if (evt.type == SDL_MOUSEMOTION) {
		float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
		float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;

		yaw = -yaw;
		pitch = -pitch;


		//update camera elevation:
//		camera->transform->rotation = glm::angleAxis(elevation + 0.5f * 3.1515926f, glm::vec3(1.0f, 0.0f, 0.0f));

//		//update player forward direction by rotation around 'up' direction:
//		glm::vec3 up = walk_mesh->world_normal(walk_point);
//		camera->transform->rotation = glm::normalize(
//				glm::angleAxis(yaw, up)
//				* camera->transform->rotation
//		);

		camera->transform->rotation = glm::normalize(
                camera->transform->rotation *
				glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f))*
						glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f))
		);

		return true;
	}

	return false;
}

void GameMode::update(float elapsed) {
	if (game_over || win) return;

    glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);

    float amt = 5.0f * elapsed;

    glm::vec3 step = glm::vec3(0.0f);

	if (controls.right) step = amt * directions[0];
	if (controls.left) step = -amt * directions[0];
	if (controls.backward) step = amt * directions[2];
	if (controls.forward) step = -amt * directions[2];

	if (step != glm::vec3(0.0f)) {
		walk_mesh->walk(walk_point, step);

		auto normal = walk_mesh->world_normal(walk_point);
		auto position = walk_mesh->world_point(walk_point) + 1.0f * normal;

		camera->transform->position.x = position.x;
		camera->transform->position.y = position.y;
		camera->transform->position.z = position.z;
	}

	auto normal = walk_mesh->world_normal(walk_point);
	glm::vec3 player_right = directions[0];
	glm::vec3 old_player_up = glm::normalize(directions[1]);
	glm::vec3 player_forward = -directions[2];

	glm::vec3 w = glm::cross(old_player_up, normal);

	glm::quat orientation_change= glm::normalize(
			glm::quat(
					glm::dot(old_player_up, normal), w.x, w.y, w.z
			)
	);

	player_forward = orientation_change * player_forward ;

	player_forward = glm::normalize(player_forward - normal * glm::dot(normal, player_forward));
	player_right = glm::cross(player_forward, normal);

	auto q = glm::quat_cast(glm::mat3(
			player_right, normal, -player_forward
	));

	camera->transform->rotation = q;

	spot->transform->rotation = camera->transform->rotation;
	spot->transform->position.x = camera->transform->position.x;
	spot->transform->position.y = camera->transform->position.y;
	spot->transform->position.z = camera->transform->position.z - 0.75f;

	for (auto spider : spiders) {
		spider->update(elapsed);
		if (glm::distance(
				glm::vec2(spider->transform->position.x, spider->transform->position.y),
				glm::vec2(camera->transform->position.x, camera->transform->position.y)) < 0.5) {
		    std::cerr << "GAME OVER" << std::endl;
		    game_over = true;
		}
	}

	if (glm::distance(
			glm::vec2(statue->position.x, statue->position.y),
			glm::vec2(camera->transform->position.x, camera->transform->position.y)
			) < 0.5) {
		win = true;
	}
}

//GameMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers {
	glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

	//This framebuffer is used for fullscreen effects:
	GLuint color_tex = 0;
	GLuint depth_rb = 0;
	GLuint fb = 0;

	//This framebuffer is used for shadow maps:
	glm::uvec2 shadow_size = glm::uvec2(0,0);
	GLuint shadow_color_tex = 0; //DEBUG
	GLuint shadow_depth_tex = 0;
	GLuint shadow_fb = 0;

	void allocate(glm::uvec2 const &new_size, glm::uvec2 const &new_shadow_size) {
		//allocate full-screen framebuffer:
		if (size != new_size) {
			size = new_size;

			if (color_tex == 0) glGenTextures(1, &color_tex);
			glBindTexture(GL_TEXTURE_2D, color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
			glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
	
			if (fb == 0) glGenFramebuffers(1, &fb);
			glBindFramebuffer(GL_FRAMEBUFFER, fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}

		//allocate shadow map framebuffer:
		if (shadow_size != new_shadow_size) {
			shadow_size = new_shadow_size;

			if (shadow_color_tex == 0) glGenTextures(1, &shadow_color_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shadow_size.x, shadow_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);


			if (shadow_depth_tex == 0) glGenTextures(1, &shadow_depth_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size.x, shadow_size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (shadow_fb == 0) glGenFramebuffers(1, &shadow_fb);
			glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_color_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth_tex, 0);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}
	}
} fbs;


void GameMode::draw(glm::uvec2 const &drawable_size) {
	fbs.allocate(drawable_size, glm::uvec2(1024, 1024));

	//Draw scene to shadow map for spotlight:
	glBindFramebuffer(GL_FRAMEBUFFER, fbs.shadow_fb);
	glViewport(0, 0, fbs.shadow_size.x, fbs.shadow_size.y);

	glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	//render only back faces to shadow map (prevent shadow speckles on fronts of objects):
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);

	scene->draw(spot, Scene::Object::ProgramTypeShadow);

	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_ERRORS();


	//Draw scene to off-screen framebuffer:
	glBindFramebuffer(GL_FRAMEBUFFER, fbs.fb);
	glViewport(0, 0, drawable_size.x, drawable_size.y);

	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(texture_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(texture_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(texture_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//use hemisphere light for subtle ambient light:
	glUniform3fv(texture_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(texture_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));
//
//	{
//	    Scene::Lamp* spot = spot_lights[0];
//		glm::mat4 world_to_spot =
//				//This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
//				glm::mat4(
//						0.5f, 0.0f, 0.0f, 0.0f,
//						0.0f, 0.5f, 0.0f, 0.0f,
//						0.0f, 0.0f, 0.5f, 0.0f,
//						0.5f, 0.5f, 0.5f + 0.00001f /* <-- bias */, 1.0f
//				)
//				//this is the world-to-clip matrix used when rendering the shadow map:
//				* spot->make_projection() * spot->transform->make_world_to_local();
//
//		glUniformMatrix4fv(texture_program->light_to_spot_mat4, 1, GL_FALSE, glm::value_ptr(world_to_spot));
//
//		glm::mat4 spot_to_world = spot->transform->make_local_to_world();
//		std::string position_name = "spot_positions[0]";
//		GLint spot_potision = glGetUniformLocation(texture_program->program, position_name.c_str());
//		glUniform3fv(spot_potision, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
//		glUniform3fv(texture_program->spot_direction_vec3, 1, glm::value_ptr(-glm::vec3(spot_to_world[2])));
//		glUniform3fv(texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
//
//		glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.5f * spot->fov), std::cos(0.85f * 0.5f * spot->fov));
//		glUniform2fv(texture_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));
//	}
	{
	    for (int i = 0; i < 12; i++) {
	    	Scene::Lamp* spot = spot_lights[i];
			glm::mat4 world_to_spot =
							  //This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
							  glm::mat4(
									  0.5f, 0.0f, 0.0f, 0.0f,
									  0.0f, 0.5f, 0.0f, 0.0f,
									  0.0f, 0.0f, 0.5f, 0.0f,
									  0.5f, 0.5f, 0.5f + 0.00001f /* <-- bias */, 1.0f
							  )
							  //this is the world-to-clip matrix used when rendering the shadow map:
							  * spot->make_projection() * spot->transform->make_world_to_local();

			std::string light_to_spot_name= "light_to_spots[" + std::to_string(i) + "]";
			GLint light_to_spot_position = glGetUniformLocation(texture_program->program, light_to_spot_name.c_str());

			glUniformMatrix4fv(light_to_spot_position, 1, GL_FALSE, glm::value_ptr(world_to_spot));

			glm::mat4 spot_to_world = spot->transform->make_local_to_world();
			std::string position_name = "spot_positions[" + std::to_string(i) + "]";
			std::string direction_name = "spot_directions[" + std::to_string(i) + "]";
			GLint spot_potision = glGetUniformLocation(texture_program->program, position_name.c_str());
			GLint spot_direction= glGetUniformLocation(texture_program->program, direction_name.c_str());
			glUniform3fv(spot_potision, 1, glm::value_ptr(glm::vec3(spot_to_world[3])));
			glUniform3fv(spot_direction, 1, glm::value_ptr(-glm::vec3(spot_to_world[2])));
			glUniform3fv(texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

			glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.4f * spot->fov), std::cos(0.85f * 0.4f * spot->fov));
			glUniform2fv(texture_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));
		}
	}
	glUniform3fv(texture_program->camera_position_vec3, 1, glm::value_ptr(camera->transform->position));

	//This code binds texture index 1 to the shadow map:
	// (note that this is a bit brittle -- it depends on none of the objects in the scene having a texture of index 1 set in their material data; otherwise scene::draw would unbind this texture):
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fbs.shadow_depth_tex);
	//The shadow_depth_tex must have these parameters set to be used as a sampler2DShadow in the shader:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	//NOTE: however, these are parameters of the texture object, not the binding point, so there is no need to set them *each frame*. I'm doing it here so that you are likely to see that they are being set.
	glActiveTexture(GL_TEXTURE0);

	scene->draw(camera);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_ERRORS();

	if (game_over) {
		std::string c = "GAME OVER";
		float height = 0.1f;
		float width = text_width(c, height);
		draw_text(c, glm::vec2(0.15f - (width + 0.02), 0.15f), height, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	}

	if (win) {
		std::string c = "YOU WIN";
		float height = 0.1f;
		float width = text_width(c, height);
		draw_text(c, glm::vec2(0.15f - (width + 0.02), 0.00f), height, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	}

	//Copy scene from color buffer to screen, performing post-processing effects:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbs.color_tex);
	glUseProgram(*blur_program);
	glBindVertexArray(*empty_vao);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
