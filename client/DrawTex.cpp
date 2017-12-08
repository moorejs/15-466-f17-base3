#include "Draw.hpp"
#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <math.h>
#include <iostream>
#include <stdexcept>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);


struct DrawTex{
    
    GLuint tex = 0;
    glm::uvec2 tex_size = glm::uvec2(0,0);
    std::vector< uint32_t > data;
    
    void loadTexture(){
        //texture:
        
        
         //load texture 'tex':
            
            if (!load_png("ui_atlas.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
                std::cerr << "Failed to load texture." << std::endl;
                exit(1);
            }
            
            glGenTextures(1, &tex);
            //create a texture object:
            //bind texture object to GL_TEXTURE_2D:
            glBindTexture(GL_TEXTURE_2D, tex);
            //upload texture data from data:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
            //set texture sampling parameters:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            
        

    }
    
    void drawtexture() {
        //Configuration:
        struct {
            glm::uvec2 size = glm::uvec2(720, 480);
        } config;
        
        
        
        //------------ opengl objects / game assets ------------
        
        
        
        //shader program:
        GLuint program = 0;
        GLuint program_Position = 0;
        GLuint program_TexCoord = 0;
        GLuint program_Color = 0;
        GLuint program_mvp = 0;
        GLuint program_tex = 0;
        GLuint program_tex2 = 0;
        { //compile shader program:
            GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
                                                  "#version 330\n"
                                                  "uniform mat4 mvp;\n"
                                                  "in vec4 Position;\n"
                                                  "in vec2 TexCoord;\n"
                                                  "in vec4 Color;\n"
                                                  "out vec2 texCoord;\n"
                                                  "out vec4 color;\n"
                                                  "void main() {\n"
                                                  "	gl_Position = mvp * Position;\n"
                                                  "	color = Color;\n"
                                                  "	texCoord = TexCoord;\n"
                                                  "}\n"
                                                  );
            
            GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
                                                    "#version 330\n"
                                                    "uniform sampler2D tex;\n"
                                                    "uniform sampler2D tex2;\n"
                                                    "in vec4 color;\n"
                                                    "in vec2 texCoord;\n"
                                                    "out vec4 fragColor;\n"
                                                    "void main() {\n"
                                                    "	fragColor = texture(tex, texCoord) * color;\n"
                                                    "	fragColor = texture(tex2, texCoord) * color;\n"
                                                    "}\n"
                                                    );
            
            program = link_program(fragment_shader, vertex_shader);
            
            //look up attribute locations:
            program_Position = glGetAttribLocation(program, "Position");
            if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
            program_TexCoord = glGetAttribLocation(program, "TexCoord");
            if (program_TexCoord == -1U) throw std::runtime_error("no attribute named TexCoord");
            program_Color = glGetAttribLocation(program, "Color");
            if (program_Color == -1U) throw std::runtime_error("no attribute named Color");
            
            //look up uniform locations:
            program_mvp = glGetUniformLocation(program, "mvp");
            if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
            program_tex = glGetUniformLocation(program, "tex");
            if (program_tex == -1U) throw std::runtime_error("no uniform named tex");
            program_tex2 = glGetUniformLocation(program, "tex2");
            if (program_tex2 == -1U) throw std::runtime_error("no uniform named tex");
        }
        
        //vertex buffer:
        GLuint buffer = 0;
        { //create vertex buffer
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
        }
        
        struct Vertex {
            Vertex(glm::vec2 const &Position_, glm::vec2 const &TexCoord_, glm::u8vec4 const &Color_) :
            Position(Position_), TexCoord(TexCoord_), Color(Color_) { }
            glm::vec2 Position;
            glm::vec2 TexCoord;
            glm::u8vec4 Color;
        };
        static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");
        
        //vertex array object:
        GLuint vao = 0;
        { //create vao and set up binding:
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0);
            glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2));
            glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
            glEnableVertexAttribArray(program_Position);
            glEnableVertexAttribArray(program_TexCoord);
            glEnableVertexAttribArray(program_Color);
        }
        
        //------------ sprite info ------------
        struct SpriteInfo {
            glm::vec2 min_uv;
            glm::vec2 max_uv;
            glm::vec2 rad;
            
            
        public:
            void setSpriteInfo(glm::vec2 min, glm::vec2 max, glm::vec2 r){
                min_uv = min;
                max_uv = max;
                rad = r;
            }
        };
        
        struct GameObject{
            
        public:
            glm::vec2 position;
            glm::vec2 min;
            glm::vec2 max;
            
            SpriteInfo sinfo;
            
            GameObject(){
                
            }
            
            GameObject(glm::vec2 pos, glm::vec2 min, glm::vec2 max, glm::vec2 scale){
                position = pos;
                sinfo.setSpriteInfo(min,max,scale);
            }
        };
        
        struct {
            glm::vec2 at = glm::vec2(0.0f, 0.0f);
            glm::vec2 radius = glm::vec2(16.0f,16.0f);
        } camera;
        //correct radius for aspect ratio:
        float aspect_ratio = (float(config.size.x) / float(config.size.y));
        camera.radius.x = camera.radius.y * aspect_ratio;
        
        GameObject bg(glm::vec2(0, 0),glm::vec2(0.0f,0.0f),glm::vec2(0.58f,1.0f),glm::vec2(24,16));
        
        
        //------------ game loop ------------
        
        //while (true) {
            
            
            
            //draw output:
//            glClearColor(0.1, 0.1, 0.1, 0.0);
//            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
            
            
//            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
//            glEnable(GL_BLEND);
//            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
            
            { //draw game state:
                std::vector< Vertex > verts;
                
                auto draw_sprite = [&verts](SpriteInfo const &sprite, glm::vec2 const &at, float angle = 0.0f) {
                    glm::vec2 min_uv = sprite.min_uv;
                    glm::vec2 max_uv = sprite.max_uv;
                    glm::vec2 rad = sprite.rad;
                    glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
                    glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
                    glm::vec2 up = glm::vec2(-right.y, right.x);
                    
                    verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
                    verts.emplace_back(verts.back());
                    verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
                    verts.emplace_back(at + right *  rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
                    verts.emplace_back(at + right *  rad.x + up *  rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
                    verts.emplace_back(verts.back());
                };
                
                
                
                //draw sprite
                draw_sprite(bg.sinfo, glm::vec2(bg.position[0] * aspect_ratio,bg.position[1]), 0.0f);
                
                //draw_sprite(randomobj.sinfo, glm::vec2(randomobj.position[0] * aspect_ratio,randomobj.position[1]), 0.0f);
                
                glBindBuffer(GL_ARRAY_BUFFER, buffer);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);
                
                glUseProgram(program);
                glUniform1i(program_tex, 0);
                glm::vec2 scale = 1.0f / camera.radius;
                glm::vec2 offset = scale * -camera.at;
                glm::mat4 mvp = glm::mat4(
                                          glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
                                          glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
                                          glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
                                          glm::vec4(offset.x, offset.y, 0.0f, 1.0f)
                                          );
                glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
                
                
                
            }
            
//            glDisable(GL_STENCIL_TEST);
        
//            SDL_GL_SwapWindow(window);
        //}
        
        
        //------------  teardown ------------
        
//        SDL_GL_DeleteContext(context);
//        context = 0;
//        
//        SDL_DestroyWindow(window);
//        window = NULL;
        
        
    }
};


static GLuint compile_shader(GLenum type, std::string const &source) {
    GLuint shader = glCreateShader(type);
    GLchar const *str = source.c_str();
    GLint length = source.size();
    glShaderSource(shader, 1, &str, &length);
    glCompileShader(shader);
    GLint compile_status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE) {
        std::cerr << "Failed to compile shader." << std::endl;
        GLint info_log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector< GLchar > info_log(info_log_length, 0);
        GLsizei length = 0;
        glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
        std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
        glDeleteShader(shader);
        throw std::runtime_error("Failed to compile shader.");
    }
    return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint link_status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE) {
        std::cerr << "Failed to link shader program." << std::endl;
        GLint info_log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
        std::vector< GLchar > info_log(info_log_length, 0);
        GLsizei length = 0;
        glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
        std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
        throw std::runtime_error("Failed to link program");
    }
    return program;
}
