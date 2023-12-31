#pragma once
#ifndef MODEL_H
#define MODEL_H
#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

extern const int roomWidth = 20.0f;
extern const int roomHeight = 14.0f;
extern const int roomDepth = 20.0f;

unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);

class Model
{
public:
    // model data 
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;
    glm::vec3 scale;        // scale factor (scaling in x, y, z)
    glm::vec3 offset;       // offset (translation in x, y, z)
    glm::mat4 rotation;
    glm::vec3 bboxMin = glm::vec3(FLT_MAX);
    glm::vec3 bboxMax = glm::vec3(FLT_MIN);
    glm::vec3 transformedMin;
    glm::vec3 transformedMax;
    glm::vec3 direction;
    const float massCenter = 0.05f;
    unsigned int textureID;
    

    // constructor, expects a filepath to a 3D model.
    Model(string const& path, bool gamma = false, glm::vec3 scale = glm::vec3(1.0f), glm::vec3 offset = glm::vec3(0.0f))
        : gammaCorrection(gamma), scale(scale), offset(offset)
    {
        loadModel(path);
        direction = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // Update the wobbling effect of the object around the Z-axis
    void updateWobbling(float deltaTime) {

        // Calculate angular acceleration based on the differential equation
        // d^2(theta)/dt^2 + b*d(theta)/dt + k*theta = 0
        // Neglecting mass (m) as it cancels out during the equation derivation for a pendulum
        alpha = (-k * theta - b * omega) / I;

        // Update angular velocity and angle using Euler's method
        omega += alpha * deltaTime;
        theta += omega * deltaTime;

        // If the angular velocity is close to zero and the object is at the bottom (theta ~ 0)
        // we assume it has stopped wobbling due to damping
        if (abs(omega) < 0.01 && abs(theta) < 0.01) {
            omega = 0.0f;
            theta = 0.0f;
            isWobbling = false; // The wobbling has stopped
        }

        rotation = glm::rotate(glm::mat4(1.0f), theta, direction); // Rotate around Z-axis
        updateTransformedBoundingBox();
    }

    void Draw(Shader& shader)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        // Combine the transformation
        model = model * rotation; // No need for an additional translation in this case

        // set the model matrix in the shader
        shader.setMat4("model", model);
        shader.setInt("reverse_normals", 0);
        // glm::vec4 pos = model * glm::vec4(0.037514f, 0.021025f, 0.024657f, 0.0f);
        // std::cout << "Draw point" << pos.x << " " << pos.y << " " << pos.z << std::endl;
        for (unsigned int i = 0; i < meshes.size(); i++) {
            // Draw the mesh with the applied model matrix
            meshes[i].Draw(shader);
        }
    }

    bool isSphereBoundingBoxIntersectingAABB(const glm::vec3& sphereCenter, float sphereRadius) {
        // 计算球体的包围盒
        glm::vec3 sphereBBoxMin = sphereCenter - glm::vec3(sphereRadius);
        glm::vec3 sphereBBoxMax = sphereCenter + glm::vec3(sphereRadius);

        // std::cout << "小球包围盒(min)：" << sphereBBoxMin.x << " " << sphereBBoxMin.y << " " << sphereBBoxMin.z << std::endl;
        // std::cout << "小球包围盒(max)：" << sphereBBoxMax.x << " " << sphereBBoxMax.y << " " << sphereBBoxMax.z << std::endl;
        // std::cout << "不倒翁围盒(min)：" << transformedMin.x << " " << transformedMin.y << " " << transformedMin.z << std::endl;
        // std::cout << "不倒翁围盒(max)：" << transformedMax.x << " " << transformedMax.y << " " << transformedMax.z << std::endl;

        // 检查球体的包围盒是否与变换后的包围盒相交
        return (sphereBBoxMin.x <= transformedMax.x && sphereBBoxMax.x >= transformedMin.x) &&
            (sphereBBoxMin.y <= transformedMax.y && sphereBBoxMax.y >= transformedMin.y) &&
            (sphereBBoxMin.z <= transformedMax.z && sphereBBoxMax.z >= transformedMin.z);
    }

    glm::vec3 transformPoint(const glm::vec3& point) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        model = model * glm::rotate(glm::mat4(1.0f), theta, direction); // No need for an additional translation in this case


        glm::vec4 transformedPoint = model * glm::vec4(point, 1.0f); // 变换点
        return glm::vec3(transformedPoint); // 转换回vec3
        // return point;
    }

    void transformPoint_2(glm::vec3& point) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        model = model * glm::rotate(glm::mat4(1.0f), theta, direction); // No need for an additional translation in this case

        point = glm::vec3(model * glm::vec4(point, 1.0f)); // 变换点
    }

    glm::vec3 getPointVelocity(const glm::vec3& point) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        // Combine the transformation
        model = model * glm::rotate(glm::mat4(1.0f), theta, direction); // No need for an additional translation in this case

        glm::vec3 transformedMassCenter = glm::vec3(model * glm::vec4(0.0f, massCenter, 0.0f, 1.0f)); // 变换点

        return cross(omega * direction, point - transformedMassCenter);
    }

    void setAngularSpeed(const glm::vec3 & speed, const glm::vec3& point, const glm::vec3& norm) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        // Combine the transformation
        model = model * glm::rotate(glm::mat4(1.0f), theta, direction); // No need for an additional translation in this case

        glm::vec3 transformedMassCenter = glm::vec3(model * glm::vec4(0.0f, massCenter, 0.0f, 1.0f)); // 变换点

        glm::vec3 arm = point - transformedMassCenter;
        arm = arm - dot(arm, direction) * direction;
        this->omega = glm::length(speed) / glm::length(arm);
        this->direction = normalize(glm::vec3(norm.x, 0.0f, norm.z));
    }

    unsigned int getTexture() const {
        return textures_loaded[0].id;
    }

    void move(glm::vec3 &newMousePoint, glm::vec3 &lastMousePoint) {
        std::cout << "move" << std::endl;
        glm::vec3 displacement = newMousePoint - lastMousePoint;
        if (transformedMin.x + displacement.x < -roomWidth / 2.0f ||
            transformedMax.x + displacement.x > roomWidth / 2.0f ||
            transformedMin.z + displacement.z < -roomDepth / 2.0f ||
            transformedMax.z + displacement.z > roomDepth / 2.0f || 
            transformedMin.y + displacement.y < -roomHeight / 2.0f ||
            transformedMax.y + displacement.y > roomHeight / 2.0f ) {
            return;
        }
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, offset);  // apply translation (offset)
        model = glm::scale(model, scale);       // apply scaling
        // Combine the transformation
        model = model * glm::rotate(glm::mat4(1.0f), theta, direction); // No need for an additional translation in this case

        glm::vec3 transformedMassCenter = glm::vec3(model * glm::vec4(0.0f, massCenter, 0.0f, 1.0f)); // 变换点

        if (newMousePoint.y < transformedMassCenter.y) {
            std::cout << "trans" << std::endl;
            offset.x += displacement.x;
            offset.z += displacement.z;
        }
        else {
            std::cout << "旋转 " << std::endl;          
            this->direction = normalize(cross(glm::vec3(0.0f, 1.0f, 0.0f),glm::vec3(displacement.x, 0.0f, displacement.z)));
            glm::vec3 arm = newMousePoint - transformedMassCenter;
            // arm = arm - dot(arm, direction) * direction;
            // displacement = displacement - dot(displacement, direction) * direction;
            // displacement = displacement - dot(displacement, arm) * arm / glm::length(arm);
            std::cout << glm::length(displacement) << " " << glm::length(arm) << " ";
            float theta_del;
            if (glm::length(arm) > 0)
                theta_del = glm::length(displacement) / (glm::length(arm) + 2.0f);
            else  theta_del = glm::length(displacement) / 5.0f;
            std::cout << theta_del;
            theta += theta_del;
        }
        updateTransformedBoundingBox();
    }

private:
    // Wobbling properties
    float theta = 0.0f; // Current angle of wobble (in radians)
    float omega = 0.0f; // Current angular velocity (in radians per second)
    float b = 0.05f;     // Damping coefficient
    float k = 2.0f;     // Spring constant for restoring torque
    float I = 0.5f;     // Moment of inertia
    // float deltaTime = 0.016f; // Time step for the simulation (1/60 seconds for 60FPS)
    bool isWobbling = true;
    float alpha;

    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const& path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);

        // 计算包围盒
        for (unsigned int k = 0; k < meshes.size(); k++) {
            for (unsigned int i = 0; i < meshes[k].vertices.size(); i++) {
                glm::vec3 point = meshes[k].vertices[i].Position;
                updateBoundingBox(point);
            }
        }
        updateTransformedBoundingBox();
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode* node, const aiScene* scene)
    {
        // process each mesh located at the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene. 
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            // texture coordinates
            if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // tangent
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // bitangent
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures);
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if (!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
            }
        }
        return textures;
    }
    
    void updateBoundingBox(const glm::vec3& point) {
        // For each component (x, y, z), find the minimum and maximum
        bboxMin.x = std::min(bboxMin.x, point.x);
        bboxMin.y = std::min(bboxMin.y, point.y);
        bboxMin.z = std::min(bboxMin.z, point.z);

        bboxMax.x = std::max(bboxMax.x, point.x);
        bboxMax.y = std::max(bboxMax.y, point.y);
        bboxMax.z = std::max(bboxMax.z, point.z);
    }

    void updateTransformedBoundingBox() {
        transformedMin = transformPoint(bboxMin);
        transformedMax = transformPoint(bboxMax);

        glm::vec3 actualMin, actualMax;

        // For each component (x, y, z), find the minimum and maximum
        actualMin.x = std::min(transformedMin.x, transformedMax.x);
        actualMin.y = std::min(transformedMin.y, transformedMax.y);
        actualMin.z = std::min(transformedMin.z, transformedMax.z);

        actualMax.x = std::max(transformedMin.x, transformedMax.x);
        actualMax.y = std::max(transformedMin.y, transformedMax.y);
        actualMax.z = std::max(transformedMin.z, transformedMax.z);

        // Set the transformed bounding box to actual minimum and maximum
        transformedMin = actualMin;
        transformedMax = actualMax;
    }
};


unsigned int TextureFromFile(const char* path, const string& directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}


#endif