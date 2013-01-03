#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
/* Glew ja inclui o gl.h */
#include <GL/glew.h>
#include <GL/glut.h>
/* SOIL - Carregamento de imagens */
#include <GL/SOIL.h>
/* GLM */
#include <../glm/glm.hpp>
#include <../glm/gtc/matrix_transform.hpp>
#include <../glm/gtc/type_ptr.hpp>
/* FONTES DE TEXTO COM FREETYPE */
#include <../freetype/ft2build.h>
#include FT_FREETYPE_H

#include "shader.h"

//Tamanho do Piso
#define PISO_SZ 15

//Tamanho inicial da tela
int screen_width=800, screen_height=600;
//Ponteiro para o executavel de shaders
GLuint program, program_for_text, program_for_ball;

//Atributos que serao usados na manipulacao de objetos e camera
GLint attribute_v_coord = -1, attribute_v_normal = -1, attribute_v_texcoords = -1, attribute_v_tangent = 1;
GLint uniform_m = -1, uniform_v = -1, uniform_p = -1;
GLint uniform_m_3x3_inv_transp = -1, uniform_v_inv = -1, uniform_mytexture = -1;
//Atributos usados nos textos
GLint attribute_coord; GLint uniform_tex; GLint uniform_color;
struct point {
	GLfloat x;
	GLfloat y;
	GLfloat s;
	GLfloat t;
};

//Atributos para o funcionamento da arcball
bool compute_arcball;
int last_mx = 0, last_my = 0, cur_mx = 0, cur_my = 0;
int arcball_on = false;

using namespace std;

//Modos de camera
//O modo camera (em cima da mesa) sera usado numa viewport e o modo object (bolinha) em outro
enum MODES { MODE_CAMERA, MODE_OBJECT, MODE_LIGHT, MODE_LAST } view_mode;
int rotY_direction = 0, rotX_direction = 0, transZ_direction = 0, strife = 0;
float speed_factor = 1;
glm::mat4 transforms[MODE_LAST];
int last_ticks = 0;

class Mesh {
private:
	//Objetos em buffer, apos calculos, nao podem ser mexidos diretamente
  GLuint vbo_vertices, vbo_normals, vbo_texcoords, vbo_tangents, ibo_elements;
public:
  vector<glm::vec4> vertices;
  vector<glm::vec3> normals;
  vector<GLushort> elements;
  vector<glm::vec2> texcoords;
  vector<glm::vec3> tangents;
  glm::mat4 object2world;
  GLuint texture_id;
  int tex_width, tex_height;

  //Construtor, cria um objeto vazio
  Mesh() : vbo_vertices(0), vbo_normals(0), vbo_texcoords(0), vbo_tangents(0),
           ibo_elements(0), object2world(glm::mat4(1)) {}
  //Destrutor: elimina tudo
  ~Mesh() {
    if (vbo_vertices != 0)
      glDeleteBuffers(1, &vbo_vertices);
    if (vbo_normals != 0)
      glDeleteBuffers(1, &vbo_normals);
    if (vbo_texcoords != 0)
      glDeleteBuffers(1, &vbo_texcoords);
    if (vbo_tangents != 0)
      glDeleteBuffers(1, &vbo_tangents);
    if (ibo_elements != 0)
      glDeleteBuffers(1, &ibo_elements);
  }

  /**
   / Upload para os buffers da GPU
   */
  void upload() {
	  //Upload os vertices
    if (this->vertices.size() > 0) {
      glGenBuffers(1, &this->vbo_vertices);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_vertices);
      glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(this->vertices[0]),
		   this->vertices.data(), GL_STATIC_DRAW);
    }
    
	//Upload das Normais
    if (this->normals.size() > 0) {
      glGenBuffers(1, &this->vbo_normals);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_normals);
      glBufferData(GL_ARRAY_BUFFER, this->normals.size() * sizeof(this->normals[0]),
		   this->normals.data(), GL_STATIC_DRAW);
    }
    
	//Upload dos indices dos elementos dos buffers
    if (this->elements.size() > 0) {
      glGenBuffers(1, &this->ibo_elements);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo_elements);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->elements.size() * sizeof(this->elements[0]),
		   this->elements.data(), GL_STATIC_DRAW);
    }

	//Upload das coordenadas de textura do Blender (nao usado ainda)
    if (this->texcoords.size() > 0) {
      glGenBuffers(1, &this->vbo_texcoords);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_texcoords);
      glBufferData(GL_ARRAY_BUFFER, this->texcoords.size() * sizeof(this->texcoords[0]),
		   this->texcoords.data(), GL_STATIC_DRAW);
    }

	//Upload da textura do objeto
	if(this->texture_id > 0) {
		glBindTexture(GL_TEXTURE_2D, this->texture_id);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
    
	//Upload das tangentes usadas na iluminacao
    if (this->tangents.size() > 0) {
      glGenBuffers(1, &this->vbo_tangents);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_tangents);
      glBufferData(GL_ARRAY_BUFFER, this->tangents.size() * sizeof(this->tangents[0]),
		   this->tangents.data(), GL_STATIC_DRAW);
    }
  }

  /**
   * Desenha o dito cujo
   */
  void draw() {
    if (this->vbo_vertices != 0) {
      glEnableVertexAttribArray(attribute_v_coord);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_vertices);
      glVertexAttribPointer(
        attribute_v_coord,  // 
        4,                  // numero de elementos por vertice (x,y,z,w)
        GL_FLOAT,           // tipo
        GL_FALSE,           // nao pre-processa os valores
        0,                  // dados extras entre as posicoes
        0                   // primeiro elemento
      );
    }

    if (this->vbo_normals != 0) {
      glEnableVertexAttribArray(attribute_v_normal);
      glBindBuffer(GL_ARRAY_BUFFER, this->vbo_normals);
      glVertexAttribPointer(
        attribute_v_normal, // 
        3,                  //  numero de elementos por vertice (x,y,z)
        GL_FLOAT,           
        GL_FALSE,           
        0,                  
        0                   
      );
    }

	//Ativa a textura no objeto
	if(this->texture_id > 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->texture_id);
		glUniform1i(uniform_mytexture, /*GL_TEXTURE*/0);
	}
    
    /* Matriz de transformação do objeto */
    glUniformMatrix4fv(uniform_m, 1, GL_FALSE, glm::value_ptr(this->object2world));
    /* Matriz de transformacao dos vetores normais */
    glm::mat3 m_3x3_inv_transp = glm::transpose(glm::inverse(glm::mat3(this->object2world)));
    glUniformMatrix3fv(uniform_m_3x3_inv_transp, 1, GL_FALSE, glm::value_ptr(m_3x3_inv_transp));
    
    /* Leva o buffer de vertices e elementos ao shader de vetices */
    if (this->ibo_elements != 0) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ibo_elements);
      int size;  glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
      glDrawElements(GL_TRIANGLES, size/sizeof(GLushort), GL_UNSIGNED_SHORT, 0);
    } else {
      glDrawArrays(GL_TRIANGLES, 0, this->vertices.size());
    }

    if (this->vbo_normals != 0)
      glDisableVertexAttribArray(attribute_v_normal);
    if (this->vbo_vertices != 0)
      glDisableVertexAttribArray(attribute_v_coord);
  }
};

//declaracao dos objetos
Mesh piso, mesa_borda, mesa_centro, bola, light_bbox;

//Carrega o arquivo do blender
//TODO: Mapas de texturas
void load_obj(const char* filename, const char* texturename, Mesh* mesh) {
  ifstream in(filename, ios::in);
  if (!in) { cout << "Nao consegui ler o arquivo OBJ: " << filename << endl; exit(1); }
  vector<int> nb_seen;

  string line;
  while (getline(in, line)) {
	  //linhas de vertices (vertex shader)
    if (line.substr(0,2) == "v ") {
      istringstream s(line.substr(2));
      glm::vec4 v; s >> v.x; s >> v.y; s >> v.z; v.w = 1.0;
      mesh->vertices.push_back(v);
	  //linhas de fragments
    }  else if (line.substr(0,2) == "f ") {
      istringstream s(line.substr(2));
      GLushort a,b,c;
      s >> a; s >> b; s >> c;
      a--; b--; c--;
      mesh->elements.push_back(a); mesh->elements.push_back(b); mesh->elements.push_back(c);
    }
    else if (line[0] == '#') { /* linhas com comentarios */ }
    else { /* qualquer outra linha */ }
  }

  //Calcula as normais
  mesh->normals.resize(mesh->vertices.size(), glm::vec3(0.0, 0.0, 0.0));
  nb_seen.resize(mesh->vertices.size(), 0);
  for (unsigned int i = 0; i < mesh->elements.size(); i+=3) {
    GLushort ia = mesh->elements[i];
    GLushort ib = mesh->elements[i+1];
    GLushort ic = mesh->elements[i+2];
    glm::vec3 normal = glm::normalize(glm::cross(
      glm::vec3(mesh->vertices[ib]) - glm::vec3(mesh->vertices[ia]),
      glm::vec3(mesh->vertices[ic]) - glm::vec3(mesh->vertices[ia])));

    int v[3];  v[0] = ia;  v[1] = ib;  v[2] = ic;
    for (int j = 0; j < 3; j++) {
      GLushort cur_v = v[j];
      nb_seen[cur_v]++;
      if (nb_seen[cur_v] == 1) {
		  //ja esta normalizado
	mesh->normals[cur_v] = normal;
      } else {
	// normalize agora
	mesh->normals[cur_v].x = mesh->normals[cur_v].x * (1.0 - 1.0/nb_seen[cur_v]) + normal.x * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v].y = mesh->normals[cur_v].y * (1.0 - 1.0/nb_seen[cur_v]) + normal.y * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v].z = mesh->normals[cur_v].z * (1.0 - 1.0/nb_seen[cur_v]) + normal.z * 1.0/nb_seen[cur_v];
	mesh->normals[cur_v] = glm::normalize(mesh->normals[cur_v]);
      }
    }
  }

  //carrega a textura usando o SOIL
  glActiveTexture(GL_TEXTURE0);
  mesh->texture_id = SOIL_load_OGL_texture(
    texturename,
    SOIL_LOAD_AUTO,
    SOIL_CREATE_NEW_ID,
    SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
	);
  //dimensoes da textura
  unsigned char* img = SOIL_load_image(texturename, &mesh->tex_width, &mesh->tex_height, NULL, 0);
}

//Inicializacao basica
int init_resources(char* vshader_filename, char* fshader_filename)
{
	//carrega os objetos do arquivo e inicializa
	load_obj("mesa-borda.obj", "mesa-borda.png", &mesa_borda);
	load_obj("mesa-centro.obj", "mesa-centro.png", &mesa_centro);
	load_obj("bola.obj", "bola.png", &bola);
  // posicao inicial eh configurada no init_view()

  //inicializa o piso
  for (int i = -PISO_SZ/2; i < PISO_SZ/2; i++) {
    for (int j = -PISO_SZ/2; j < PISO_SZ/2; j++) {
      piso.vertices.push_back(glm::vec4(i,   0.0,  j+1, 1.0));
      piso.vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      piso.vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      piso.vertices.push_back(glm::vec4(i,   0.0,  j,   1.0));
      piso.vertices.push_back(glm::vec4(i+1, 0.0,  j+1, 1.0));
      piso.vertices.push_back(glm::vec4(i+1, 0.0,  j,   1.0));
      for (unsigned int k = 0; k < 6; k++)
	piso.normals.push_back(glm::vec3(0.0, 1.0, 0.0));
    }
  }
  //textura do piso
    //carrega a textura usando o SOIL
  glActiveTexture(GL_TEXTURE0);
  piso.texture_id = SOIL_load_OGL_texture(
    "piso.jpg",
    SOIL_LOAD_AUTO,
    SOIL_CREATE_NEW_ID,
    SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
	);
  //dimensoes da textura
  unsigned char* img = SOIL_load_image("piso.jpg", &piso.tex_width, &piso.tex_height, NULL, 0);

  //inicializa a fonte de luz
  glm::vec3 light_position = glm::vec3(0.0,  1.0,  2.0);
  light_bbox.vertices.push_back(glm::vec4(-0.1, -0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4( 0.1, -0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4( 0.1,  0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4(-0.1,  0.1, -0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4(-0.1, -0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4( 0.1, -0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4( 0.1,  0.1,  0.1, 0.0));
  light_bbox.vertices.push_back(glm::vec4(-0.1,  0.1,  0.1, 0.0));
  light_bbox.object2world = glm::translate(glm::mat4(1), light_position);

  mesa_borda.upload();
  mesa_centro.upload();
  bola.upload();
  piso.upload();
  light_bbox.upload();
 

  /* Compile and link shaders */
  GLint link_ok = GL_FALSE;
  GLint validate_ok = GL_FALSE;

  GLuint vs, fs;
  if ((vs = create_shader(vshader_filename, GL_VERTEX_SHADER))   == 0) return 0;
  if ((fs = create_shader(fshader_filename, GL_FRAGMENT_SHADER)) == 0) return 0;

  //Link do shader
  program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
  if (!link_ok) {
    fprintf(stderr, "glLinkProgram:");
    print_log(program);
    return 0;
  }

  //Valida o linking
  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &validate_ok);
  if (!validate_ok) {
    fprintf(stderr, "glValidateProgram:");
    print_log(program);
  }

  //Bind das coordenadas
  const char* attribute_name;
  attribute_name = "v_coord";
  attribute_v_coord = glGetAttribLocation(program, attribute_name);
  if (attribute_v_coord == -1) {
    fprintf(stderr, "Nao consegui capturar o attribute %s\n", attribute_name);
    return 0;
  }
  //Bind das normais
  attribute_name = "v_normal";
  attribute_v_normal = glGetAttribLocation(program, attribute_name);
  if (attribute_v_normal == -1) {
    fprintf(stderr, "Nao consegui capturar o attribute %s\n", attribute_name);
    return 0;
  }
  //Bind do uniform do model
  //Definicao de uniform:
  //A uniform is a global GLSL variable declared with the "uniform" storage qualifier. These act as parameters that the user of a shader program can pass to that program. They are stored in a program object. 
  const char* uniform_name;
  uniform_name = "m";
  uniform_m = glGetUniformLocation(program, uniform_name);
  if (uniform_m == -1) {
    fprintf(stderr, "Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }

  //Bind do uniform da view
  uniform_name = "v";
  uniform_v = glGetUniformLocation(program, uniform_name);
  if (uniform_v == -1) {
    fprintf(stderr, "Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }
  //BInd do uniform da projection
  uniform_name = "p";
  uniform_p = glGetUniformLocation(program, uniform_name);
  if (uniform_p == -1) {
    fprintf(stderr, "Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }
  //Bind da operacao de transposicao de matrizes 3x3
  uniform_name = "m_3x3_inv_transp";
  uniform_m_3x3_inv_transp = glGetUniformLocation(program, uniform_name);
  if (uniform_m_3x3_inv_transp == -1) {
    fprintf(stderr,"Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }
  //Invesao de vetor
  uniform_name = "v_inv";
  uniform_v_inv = glGetUniformLocation(program, uniform_name);
  if (uniform_v_inv == -1) {
    fprintf(stderr, "Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }
  //Acesso a textura
  uniform_name = "mytexture";
  uniform_mytexture = glGetUniformLocation(program, uniform_name);
  if (uniform_mytexture == -1) {
    fprintf(stderr, "Nao consegui capturar o uniform %s\n", uniform_name);
    return 0;
  }

  return 1;
}

void init_view() {
  mesa_centro.object2world = glm::mat4(1);
  mesa_borda.object2world = glm::mat4(1);
  bola.object2world = glm::mat4(1);
  //Inicia a famosa camera
  //GL2 nao implementa o lookat, precisamos do GLM
  transforms[MODE_CAMERA] = glm::lookAt(
    glm::vec3(0.0,  8.0, 8.0),   // eye
    glm::vec3(0.0,  0.0, 0.0),   // direction
    glm::vec3(0.0,  0.5, 0.0));  // up
}

void onSpecial(int key, int x, int y) {
  int modifiers = glutGetModifiers();
  if ((modifiers & GLUT_ACTIVE_ALT) == GLUT_ACTIVE_ALT)
    strife = 1;
  else
    strife = 0;

  if ((modifiers & GLUT_ACTIVE_SHIFT) == GLUT_ACTIVE_SHIFT)
    speed_factor = 0.1;
  else
    speed_factor = 1;

  switch (key) {
  case GLUT_KEY_F1:
    view_mode = MODE_OBJECT;
    break;
  case GLUT_KEY_F2:
    view_mode = MODE_CAMERA;
    break;
  case GLUT_KEY_LEFT:
    rotY_direction = 1;
    break;
  case GLUT_KEY_RIGHT:
    rotY_direction = -1;
    break;
	//cabeca pra baixo
  case GLUT_KEY_UP:
    rotX_direction = -1;
    break;
	//cabeca pra cima
  case GLUT_KEY_DOWN:
    rotX_direction = 1;
    break;
	//reseta a bicharada
  case GLUT_KEY_HOME:
    init_view();
    break;
  }
}

void onSpecialUp(int key, int x, int y) {
  switch (key) {
  case GLUT_KEY_LEFT:
  case GLUT_KEY_RIGHT:
    rotY_direction = 0;
    break;
  case GLUT_KEY_UP:
  case GLUT_KEY_DOWN:
    transZ_direction = 0;
    break;
  case GLUT_KEY_PAGE_UP:
  case GLUT_KEY_PAGE_DOWN:
    rotX_direction = 0;
    break;
  }
}

/**
 * Get a normalized vector from the center of the virtual ball O to a
 * point P on the virtual ball surface, such that P is aligned on
 * screen's (X,Y) coordinates.  If (X,Y) is too far away from the
 * sphere, return the nearest point on the virtual ball surface.
 */
glm::vec3 get_arcball_vector(int x, int y) {
  glm::vec3 P = glm::vec3(1.0*x/screen_width*2 - 1.0,
			  1.0*y/screen_height*2 - 1.0,
			  0);
  P.y = -P.y;
  float OP_squared = P.x * P.x + P.y * P.y;
  if (OP_squared <= 1*1)
    P.z = sqrt(1*1 - OP_squared);  // Pythagore
  else
    P = glm::normalize(P);  // nearest point
  return P;
}

void logic() {
  /* Movimentacao de camera no teclado */
	//Calcula o tempo decorrido para movimentacao suave
  int delta_t = glutGet(GLUT_ELAPSED_TIME) - last_ticks;
  last_ticks = glutGet(GLUT_ELAPSED_TIME);

  //Calcula o delta da movimentacao baseado na diferenca do tempo, com controle de suavidade
  float delta_transZ = transZ_direction * delta_t / 1000.0 * 3 * speed_factor;  // 3 unidade de movimenacao
  float delta_transX = 0, delta_transY = 0, delta_rotY = 0, delta_rotX = 0;
  if (strife) {
    delta_transX = rotY_direction * delta_t / 1000.0 * 5 * speed_factor;  // 5 unidade/s
    delta_transY = rotX_direction * delta_t / 1000.0 * 5 * speed_factor;  // 5 unidades/s
  } else {
	  //se nao for strife, gira em 120 graus por segundo
    delta_rotY =  rotY_direction * delta_t / 1000.0 * 120 * speed_factor; 
    delta_rotX = -rotX_direction * delta_t / 1000.0 * 120 * speed_factor;
  }
  
  //Movimenta a bicharada com os deltas ja calculados
  if (view_mode == MODE_OBJECT) {
    mesa_borda.object2world = glm::rotate(mesa_borda.object2world, delta_rotY, glm::vec3(0.0, 1.0, 0.0));
    mesa_borda.object2world = glm::rotate(mesa_borda.object2world, delta_rotX, glm::vec3(1.0, 0.0, 0.0));
    mesa_borda.object2world = glm::translate(mesa_borda.object2world, glm::vec3(0.0, 0.0, delta_transZ));
	mesa_centro.object2world = glm::rotate(mesa_centro.object2world, delta_rotY, glm::vec3(0.0, 1.0, 0.0));
    mesa_centro.object2world = glm::rotate(mesa_centro.object2world, delta_rotX, glm::vec3(1.0, 0.0, 0.0));
    mesa_centro.object2world = glm::translate(mesa_centro.object2world, glm::vec3(0.0, 0.0, delta_transZ));
	bola.object2world = glm::rotate(bola.object2world, delta_rotY, glm::vec3(0.0, 1.0, 0.0));
    bola.object2world = glm::rotate(bola.object2world, delta_rotX, glm::vec3(1.0, 0.0, 0.0));
    bola.object2world = glm::translate(bola.object2world, glm::vec3(0.0, 0.0, delta_transZ));
  } else if (view_mode == MODE_CAMERA) {
	  //Modo camera, movimentamos o MUNDO na verdade
    if (strife) {
      transforms[MODE_CAMERA] = glm::translate(glm::mat4(1.0), glm::vec3(delta_transX, 0.0, 0.0)) * transforms[MODE_CAMERA];
    } else {
      glm::vec3 y_axis_world = glm::mat3(transforms[MODE_CAMERA]) * glm::vec3(0.0, 1.0, 0.0);
      transforms[MODE_CAMERA] = glm::rotate(glm::mat4(1.0), -delta_rotY, y_axis_world) * transforms[MODE_CAMERA];
    }

    if (strife)
      transforms[MODE_CAMERA] = glm::translate(glm::mat4(1.0), glm::vec3(0.0, delta_transY, 0.0)) * transforms[MODE_CAMERA];
    else
      transforms[MODE_CAMERA] = glm::translate(glm::mat4(1.0), glm::vec3(0.0, 0.0, delta_transZ)) * transforms[MODE_CAMERA];

     transforms[MODE_CAMERA] = glm::rotate(glm::mat4(1.0), delta_rotX, glm::vec3(1.0, 0.0, 0.0)) * transforms[MODE_CAMERA];
  }

  /* Handle arcball */
  if (cur_mx != last_mx || cur_my != last_my) {
    glm::vec3 va = get_arcball_vector(last_mx, last_my);
    glm::vec3 vb = get_arcball_vector( cur_mx,  cur_my);
    float angle = acos(min(1.0f, glm::dot(va, vb)));
    glm::vec3 axis_in_camera_coord = glm::cross(va, vb);
    glm::mat3 camera2object = glm::inverse(glm::mat3(transforms[MODE_CAMERA]) * glm::mat3(mesa_borda.object2world));
    glm::vec3 axis_in_object_coord = camera2object * axis_in_camera_coord;
    mesa_borda.object2world = glm::rotate(mesa_borda.object2world, glm::degrees(angle), axis_in_object_coord);
	mesa_centro.object2world = glm::rotate(mesa_centro.object2world, glm::degrees(angle), axis_in_object_coord);
	bola.object2world = glm::rotate(bola.object2world, glm::degrees(angle), axis_in_object_coord);
	piso.object2world = glm::rotate(bola.object2world, glm::degrees(angle), axis_in_object_coord);
    last_mx = cur_mx;
    last_my = cur_my;
  }

  // Model
  // Vai no onDisplay(), exemplo: mesa_borda.object2world

  // View
  glm::mat4 world2camera = transforms[MODE_CAMERA];

  // Projection
  glm::mat4 camera2screen = glm::perspective(45.0f, 1.0f*screen_width/screen_height, 0.1f, 100.0f);

  //Avisa para usar o programa compilado
  glUseProgram(program);
  glUniformMatrix4fv(uniform_v, 1, GL_FALSE, glm::value_ptr(world2camera));
  glUniformMatrix4fv(uniform_p, 1, GL_FALSE, glm::value_ptr(camera2screen));

  glm::mat4 v_inv = glm::inverse(world2camera);
  glUniformMatrix4fv(uniform_v_inv, 1, GL_FALSE, glm::value_ptr(v_inv));

  glutPostRedisplay();
}


//Desenha a bicharada na tela
void draw() {
  glClearColor(0.45, 0.45, 0.45, 1.0);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(program);

  mesa_borda.draw();
  mesa_centro.draw();
  bola.draw();
  piso.draw();
}


//Callback de display do GL
void onDisplay()
{
  logic();
  draw();
  glutSwapBuffers();
}

void onMouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    arcball_on = true;
    last_mx = cur_mx = x;
    last_my = cur_my = y;
  } else {
    arcball_on = false;
  }
   //scroll do mouse
  if ((button == 3) || (button == 4)) {
	  if (state == GLUT_UP) {
		  transZ_direction = 0;
	  } 
	   else {
		  speed_factor = 1;
		  transZ_direction = 1;
	   }
       printf("Scroll %s posicao %d %d\n", (button == 3) ? "Up" : "Down", x, y);
   }

}

void onMotion(int x, int y) {
  if (arcball_on) {  // if left button is pressed
    cur_mx = x;
    cur_my = y;
  }
}

void onReshape(int width, int height) {
  screen_width = width;
  screen_height = height;
  glViewport(0, 0, screen_width, screen_height);
}

void free_resources()
{
  glDeleteProgram(program);
}


int main(int argc, char* argv[]) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA|GLUT_ALPHA|GLUT_DOUBLE|GLUT_DEPTH);
  glutInitWindowSize(screen_width, screen_height);
  glutCreateWindow("RodrigoDK Billiard");

  GLenum glew_status = glewInit();
  if (glew_status != GLEW_OK) {
    fprintf(stderr, "Erro de carregamento do GL : %s\n", glewGetErrorString(glew_status));
    return 1;
  }

  if (!GLEW_VERSION_2_0) {
    fprintf(stderr, "Erro: nao consegui encontrar suporte opnegl 2.0\n");
    return 1;
  }

  char* v_shader_filename = (char*) "phong-shading.vertex";
  char* f_shader_filename = (char*) "phong-shading.fragment";

  //se carregou os objetos com sucesso
  if (init_resources(v_shader_filename, f_shader_filename)) {
	  //reseta a posicao dos objetos no mundo
	  init_view();
    glutDisplayFunc(onDisplay);
	//cuida dos comandos de teclado (teclas especiais como setas)
    glutSpecialFunc(onSpecial);
    glutSpecialUpFunc(onSpecialUp);
	//cuida dos comandos de mouse
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
	//modificacoes do tamanho da janela
    glutReshapeFunc(onReshape);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
	//controle de tempo
    last_ticks = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
  }

  free_resources();
  cin.get();
  return 0;
}
