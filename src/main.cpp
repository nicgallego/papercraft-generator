// This example is heavily based on the tutorial at https://open.gl

#include <iostream>
#include <math.h>
#include <vector>
#include <list> 
#include <algorithm>
#include <fstream>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <cassert>

// OpenGL Helpers to reduce the clutter
#include "Helpers.h"

// GLFW is necessary to handle the OpenGL context
#include <GLFW/glfw3.h>

// Linear Algebra Library
#include <Eigen/Core>

// Timer
#include <chrono>

// Contains the vertex positions
Eigen::MatrixXf V(2,3);

// Contains the per-vertex color
Eigen::MatrixXf C(3,3);

// The view matrix
// Eigen::MatrixXf ViewMat(4,4);
Eigen::MatrixXf ProjectMat(4,4);

// Color, order in Key1 to Key9
Eigen::Vector3i RED(255, 0, 0);
Eigen::Vector3i GREEN(0, 255, 0);
Eigen::Vector3i BLUE(0, 0, 255); // Selected color
Eigen::Vector3i YELLOW(255, 255, 0);
Eigen::Vector3i BLACK(0, 0, 0);
Eigen::Vector3i WHITE(255, 255, 255);
Eigen::Vector3i GREY(128,128,128);
Eigen::Vector3i ORANGE(255, 165, 0);
Eigen::Vector3i PURPLE(160, 32, 240);
Eigen::Vector3i DIMGREY(105,105,105);
Eigen::Vector3i LIGHTGREY(200,200,200);
std::vector<Eigen::Vector3i> colors = {RED, GREEN, YELLOW, WHITE, GREY, ORANGE, PURPLE};

// Flags
float pre_aspect_ratio = -1;
bool pre_aspect_is_x = false;
bool drag = false;
float hit_dist;
Eigen::Vector4f pre_cursor_point;

class Light {
    public:
        Eigen::Vector4f position;
        Eigen::Vector3f intensity; // default (1,1,1)
        Light(Eigen::Vector4f position, Eigen::Vector3f intensity = Eigen::Vector3f(1,1,1)) {
            this->position = position; this->intensity = intensity;
        }
};
class Camera {
    public:
        Eigen::Matrix4f ViewMat;
        Eigen::Matrix4f ortho_mat;
        Eigen::Matrix4f perspect_mat;
        Eigen::Vector3f position;
        Eigen::Vector3f global_up;
        Eigen::Vector3f up;
        Eigen::Vector3f right;
        Eigen::Vector3f forward;
        Eigen::Vector3f target;

        // bounding box
        float n, f, t, b, r, l;
        int project_mode;
        float theta;
        int phi, zeta;
        float radius;

        Camera(GLFWwindow* window, Eigen::Vector3f position=Eigen::Vector3f(0.0, 0.0, 3.0), int project_mode = ORTHO) {
            // set GLOBAL UP to y-axis as default, set look at target as origin(0,0,0)
            this->global_up = Eigen::Vector3f(0.0, 1.0, 0.0);
            this->n = 2.0; this->f = 100.0;
            // FOV angle is hardcoded to 60 degrees
            this->theta = (PI/180) * 60;
            // trackball
            this->target = Eigen::Vector3f(0.0, 0.0, 0.0);
            this->phi = 0;
            this->zeta = 0;
            this->radius = 3.0;

            this->update_camera_pos();
            this->look_at(window);
            this->project_mode = project_mode;
        }
        void update_camera_pos() {
            float rphi = (PI/180.0)*this->phi, rzeta = (PI/180.0)*this->zeta;
            float y = this->radius * sin(rphi);
            float x = this->radius * sin(rzeta) * cos(rphi);
            float z = this->radius * cos(rzeta) * cos(rphi);

            this->position = Eigen::Vector3f(x, y, z);
        }
        void rotate(int dPhi, int dZeta) {
            this->phi = (this->phi+dPhi+360) % 360;
            this->zeta = (this->zeta+dZeta+360) % 360;
            this->update_camera_pos();
        }
        void zoom(double factor) {
            this->radius *= factor;
            this->update_camera_pos();
        }
        void reset() {
            this->phi = 0;
            this->zeta = 0;
            this->update_camera_pos();
        }
        void look_at(GLFWwindow* window, Eigen::Vector3f target = Eigen::Vector3f(0.0, 0.0, 0.0)) {
            this->forward = (this->position - this->target).normalized();
            // special case when forward is parallel to global up
            float dotVal = this->global_up.dot(this->forward);
            if (this->phi == 90 || this->phi == 270) {
                this->right = (this->up.cross(this->forward).normalized());
            }
            else if (this->phi > 90 && this->phi < 270) {
                this->right = (-this->global_up.cross(this->forward).normalized());
            }
            else {
                this->right = (this->global_up.cross(this->forward).normalized());
            }
            this->up = this->forward.cross(this->right);

            auto w = this->forward, u = this->right, v = this->up;
            Eigen::Matrix4f LOOK;
            LOOK <<
            u(0), u(1), u(2), 0.0,
            v(0), v(1), v(2), 0.0,
            w(0), w(1), w(2), 0.0,
            0.0,   0.0, 0.0,  1.0;

            Eigen::Matrix4f AT;
            AT <<
            1.0, 0.0, 0.0, -this->position(0),
            0.0, 1.0, 0.0, -this->position(1),
            0.0, 0.0, 1.0, -this->position(2),
            0.0, 0.0, 0.0, 1.0;

            this->ViewMat = LOOK * AT;

            this->update_project_mat(window);
        }
        Eigen::Vector3f to_world_point(Eigen::Vector3f screen_point) {
            Eigen::Vector3f global_x, global_y, global_z;
            global_x << 1.0, 0.0, 0.0;
            global_y << 0.0, 1.0, 0.0;
            global_z << 0.0, 0.0, 1.0;
            double xworld = screen_point.dot(global_x);
            double yworld = screen_point.dot(global_y);
            double zworld = screen_point.dot(global_z);
            return Eigen::Vector3f(xworld, yworld, zworld);
        }
        void switch_project_mode() {
            this->project_mode *= -1;
        }
        void update_project_mat(GLFWwindow* window) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            double halfWidth = width*0.5f;
            double aspect = (double)width/(double)height;
            this->t = tan(theta/2) * std::abs(n);
            this->b = -this->t;
            this->r = aspect * this->t;
            this->l = -this->r;

            this->ortho_mat << 
            2.0/(r-l), 0.0, 0.0, -(r+l)/(r-l),
            0.0, 2.0/(t-b), 0.0, -(t+b)/(t-b),
            0.0, 0.0, -2.0/(f-n), -(f+n)/(f-n),
            // 0.0, 0.0, -2.0/(f-n), -(n+f)/(n-f),
            0.0, 0.0, 0.0, 1.0;

            this->perspect_mat <<
            2*n/(r-l), 0.,      (r+l)/(r-l),    0.,
            0., (2*n)/(t-b),    (t+b)/(t-b),    0.,
            0., 0.,             -(f+n)/(f-n), (-2*f*n)/(f-n),
            0., 0.,             -1.,            0;
        }
        Eigen::Matrix4f get_project_mat() {
            if (this->project_mode == ORTHO) return this->ortho_mat;
            return this->perspect_mat;
        }
};
class RayTracer {
    public:
        std::vector<Light*> lights;
        void add_light(Eigen::Vector4f position) {
            this->lights.push_back(new Light(position));
        }
        Eigen::Vector3f get_diffuse_color(Eigen::Vector4f vpos, Eigen::Vector4f meshNormals, Eigen::Vector3f color, Eigen::MatrixXf ModelMat) {
            auto light = this->lights[0];
            // ambinet
            float ambientStrength = 0.01;
            Eigen::Vector3f ambient = ambientStrength * light->intensity;
            //diffuse
            Eigen::Matrix3f diffuse_mat;
            diffuse_mat <<  color(0), 0, 0,
                            0, color(1), 0,
                            0, 0, color(2);
            // Eigen::Matrix4f normalMatrix = (ModelMat.inverse()).transpose();
            // Eigen::Vector4f normal = (normalMatrix * meshNormals).normalized();
            Eigen::Vector4f normal = meshNormals;
            Eigen::Vector4f surfaceToLight = light->position - vpos;
            // float brightness = normal.dot(surfaceToLight) / (surfaceToLight.norm() * surfaceToLight.norm());
            float brightness = normal.dot(surfaceToLight);
            brightness = fmax(brightness, 0.);
            Eigen::Vector3f diffuse = diffuse_mat * light->intensity * brightness;
            Eigen::Vector3f result = ambient + diffuse;
            return result;
        }
};
class BezierCurve {
    public:
        Eigen::MatrixXf V;
        VertexArrayObject VAO;
        VertexBufferObject VBO_P;

        Eigen::MatrixXf curve_points;
        VertexArrayObject VAO_C;
        VertexBufferObject VBO_CP;

        int nxt_curve_pt;
        bool anime_finish;

        BezierCurve() {
            this->V.resize(4, 0);
            // Create a VAO
            this->VAO.init();
            this->VAO.bind();
            // Initialize the VBO with the vertices data
            this->VBO_P.init();

            this->curve_points.resize(4, 0);
            // Create a VAO
            this->VAO_C.init();
            this->VAO_C.bind();
            // Initialize the VBO with the vertices data
            this->VBO_CP.init();

            nxt_curve_pt = 0;
            anime_finish = true;
        }
        void insert_point(double x, double y, double z) {
            // this->control_points.push_back(new Circle(x, y, z));
            this->V.conservativeResize(V.rows(), V.cols()+1);
            this->V.col(V.cols()-1) << x, y, z, 1.;
            this->VBO_P.update(this->V);
            this->update_BP();
        }
        void update_BP() {
            if (this->V.cols() <= 0) return;
            double delta = 0.01;
            int line_amount = 1/delta;
            this->curve_points.resize(curve_points.rows(), line_amount+1);
            for (int i = 0; i <= line_amount; i += 1) {
                double t = i*delta;
                Eigen::Vector4f p = get_BP_at(t);
                this->curve_points.col(i) << Eigen::Vector4f(p(0), p(1), p(2), 1.);
                // this->curve_points.col(i) << p;
            }
            this->VBO_CP.update(this->curve_points);
        }
        Eigen::Vector4f get_BP_at(double t) {
            Eigen::MatrixXf CP = this->V;
            return get_BP(t, CP, CP.cols());
        }
        Eigen::Vector4f get_BP(double t, Eigen::MatrixXf CP, int n) {
            if (n == 1) return CP.col(0);
            for (int i = 0; i < n-1; i++) {
                CP.col(i) = (1-t)*CP.col(i) + t*CP.col(i+1);
            }
            return get_BP(t, CP, n-1);
        }
        bool translate(Eigen::Vector4f delta, int point_idx) {
            if (point_idx >= this->V.cols()) return false;
            this->V.col(point_idx) += delta;
            // this->control_points[point_idx]->translate(delta(0), delta(1), delta(2));
            this->VBO_P.update(this->V);
            this->update_BP();
            return true;
        }
};
class Mesh {
    public:
        Eigen::Vector3f color;

        Eigen::Vector4f centroid;
        double r, s, tx, ty;
        Eigen::Vector4f normal;

        std::map<int, Eigen::Vector3f> vid2v;
        std::map<int, Eigen::Vector3f> vid2fv;
        std::vector<int> vids;
        std::vector< std::pair< Mesh*, std::pair<int,int> > > nebMeshes;
        int id;

        VertexArrayObject VAO;
        VertexBufferObject VBO_P;

        Mesh(int id, Eigen::MatrixXf V, int v1, int v2, int v3, Eigen::Vector3i color=LIGHTGREY) {
            this->id = id;
            this->color = color.cast<float>();
            this->vids.push_back(v1); this->vids.push_back(v2); this->vids.push_back(v3);
            this->vid2v[v1] = V.col(v1); this->vid2v[v2] = V.col(v2); this->vid2v[v3] = V.col(v3);
            // init flat position
            Eigen::Vector3f zero = Eigen::VectorXf::Zero(3);
            this->vid2fv[v1] = zero; this->vid2fv[v2] = zero; this->vid2fv[v3] = zero;

            this->VAO.init();
            this->VAO.bind();
            this->VBO_P.init();
            this->updateVBOP();
        }
        void updateVBOP() {
            int v1 = vids[0], v2 = vids[1], v3 = vids[2];
            Eigen::Matrix3f fV;
            fV << vid2fv[v1], vid2fv[v2], vid2fv[v3];
            std::cout << "mesh flat V" << std::endl;
            std::cout << fV << std::endl;
            this->VBO_P.update(fV);
        }
        Eigen::Matrix3f getFlatV() {
            int v1 = vids[0], v2 = vids[1], v3 = vids[2];
            Eigen::Matrix3f fV;
            fV << vid2fv[v1], vid2fv[v2], vid2fv[v3];
            return fV;
        }
        Mesh(Eigen::MatrixXf V, Eigen::MatrixXf bounding_box, Eigen::Vector3i color=LIGHTGREY) {
            this->r = 0; this->s = 1; this->tx = 0; this->ty = 0;
            this->color = color.cast<float>();
            
            auto a = to_3(V.col(0)), b = to_3(V.col(1)), c = to_3(V.col(2));
            auto tmp = ((b-a).cross(c-a)).normalized();
            this->normal = Eigen::Vector4f(tmp(0), tmp(1), tmp(2), 0.0);
            // Computer the barycenter(centroid) of the Mesh, alphea:beta:gamma = 1:1:1
            Eigen::Vector4f A = V.col(0), B = V.col(1), C = V.col(2);
            this->centroid = (1.0/3)*A + (1.0/3)*B + (1.0/3)*C;
        }
        bool getIntersection(Eigen::Vector4f ray_origin, Eigen::Vector4f ray_direction, Eigen::Vector4f &intersection, Eigen::MatrixXf V) {
            // solve equation:     e + td = a + u(b-a) + v(c-a)
            //                     (a-b)u + (a-c)v + dt = a-e
            Eigen::Vector4f a = V.col(0), b = V.col(1), c = V.col(2);
            Eigen::Vector4f d = ray_direction, e = ray_origin;
            double ai = (a-b)(0), di = (a-c)(0), gi = (d)(0);
            double bi = (a-b)(1), ei = (a-c)(1), hi = (d)(1);
            double ci = (a-b)(2), fi = (a-c)(2), ii = (d)(2);
            double ji = (a-e)(0), ki = (a-e)(1), li = (a-e)(2);
            double M = ai*(ei*ii-hi*fi)+bi*(gi*fi-di*ii)+ci*(di*hi-ei*gi);

            double xt = -(fi*(ai*ki-ji*bi)+ei*(ji*ci-ai*li)+di*(bi*li-ki*ci))/M;
            if (xt <= 0) return false;

            double xu = (ji*(ei*ii-hi*fi)+ki*(gi*fi-di*ii)+li*(di*hi-ei*gi))/M;
            if (xu < 0 || xu > 1) return false;

            double xv = (ii*(ai*ki-ji*bi)+hi*(ji*ci-ai*li)+gi*(bi*li-ki*ci))/M;
            
            // intersection inside the Mesh
            intersection = ray_origin + xt*ray_direction;
            intersection(3) = 1.0;

            if (xv < 0 || xv+xu > 1) return false;
            return true;
        }
};
class FlattenObject {
    public:
        std::vector<Mesh*> meshes;
        std::map<std::pair<int, int>, std::vector<Mesh*>> edge2meshes;
        std::map<std::pair<int, int>, double> edge2weight;

        Eigen::MatrixXf V;
        Eigen::VectorXi IDX;

        VertexArrayObject VAO;
        VertexBufferObject VBO_P;
        IndexBufferObject IBO_IDX;

        void addEdge(int v1, int v2, Mesh* mesh) {
            auto edge = v1 < v2? std::make_pair(v1, v2) : std::make_pair(v2, v1);
            edge2meshes[edge].push_back(mesh);
            if (edge2weight.find(edge) == edge2weight.end())
                edge2weight[edge] = (V.col(v1)-V.col(v2)).norm();
        }
        void addNebMeshes(int v1, int v2, Mesh* mesh) {
            auto edge = v1 < v2? std::make_pair(v1, v2) : std::make_pair(v2, v1);
            for (auto nebMesh: edge2meshes[edge]) {
                if (nebMesh != mesh) {
                    mesh->nebMeshes.push_back(std::make_pair(nebMesh, edge));
                    std::cout << mesh->id << "->" << nebMesh->id << std::endl;
                }
            }
            std::cout << mesh->nebMeshes.size() << std::endl;
        }
        bool flattenFirst(Mesh* mesh, std::set<Mesh*> &flatten) {
            int v1 = mesh->vids[0], v2 = mesh->vids[1], v3 = mesh->vids[2];
            float v1v2Len = (mesh->vid2v[v1] - mesh->vid2v[v2]).norm();
            mesh->vid2fv[v1] = Eigen::Vector3f(0., 0., 0.);
            mesh->vid2fv[v2] = Eigen::Vector3f(0., v1v2Len, 0.);
            Eigen::Vector3f flatPos;
            if (!flattenVertex(mesh, v3, v1, v2, mesh->vid2fv[v1], mesh->vid2fv[v2], flatPos, flatten)) {
                return false;
            }
            mesh->vid2fv[v3] = flatPos;
            std::cout << "first mesh third point" << std::endl;
            std::cout << mesh->vid2fv[v3] << std::endl;
            return true;
        }
        FlattenObject(Eigen::MatrixXf V, Eigen::VectorXi IDX) {
            V.conservativeResize(3, V.cols());
            this->V = V;

            // create V and F matrix
            int cidx = 0;
            for (int i = 0; i < IDX.rows(); i += 3) {
                int v1 = IDX(i), v2 = IDX(i+1), v3 = IDX(i+2);
                Mesh* newMesh = new Mesh(i/3, V, v1, v2, v3, colors[cidx++ % colors.size()]);
                meshes.push_back(newMesh);
                // add edge
                addEdge(v1, v2, newMesh);
                addEdge(v2, v3, newMesh);
                addEdge(v1, v3, newMesh);
                std::cout << "id " << newMesh->id << std::endl;
                std::cout << "v1, v2, v3" << std::endl;
                std::cout << v1 << " " << v2 << " " << v3 << std::endl;
            }

            std::cout << "created meshes and edge to meshes" << std::endl;
            std::cout << "face #: " << meshes.size() << std::endl;
            std::cout << "edge #: " << edge2meshes.size() << std::endl;

            // add edge field to mesh objects
            for (auto mesh: meshes) {
                int v1 = mesh->vids[0], v2 = mesh->vids[1], v3 = mesh->vids[2];
                addNebMeshes(v1, v2, mesh);
                addNebMeshes(v2, v3, mesh);
                addNebMeshes(v1, v3, mesh);
                assert(mesh->nebMeshes.size() == 3);
            }

            std::cout << "created meshes to nebs" << std::endl;

            // BFS
            std::queue<Mesh*> q;
            std::set<Mesh*> flattened;

            // flat first mesh
            flattenFirst(meshes[0], flattened);
            flattened.insert(meshes[0]);
            q.push(meshes[0]);
            std::cout << "flattened first mesh" << std::endl;
            // std::cout << "mesh flat V" << std::endl;
            // std::cout << meshes[0]->getFlatV() << std::endl;
            
            while (!q.empty()) {
                Mesh* curMesh = q.front();
                q.pop();
                for(auto meshNedge: curMesh->nebMeshes) {
                    Mesh* nebMesh = meshNedge.first;
                    auto edge = meshNedge.second;
                    if (flattened.find(nebMesh) == flattened.end() && flattenMesh(curMesh, nebMesh, edge, flattened)) {
                        // std::cout << "inqueue " << nebMesh->id << std::endl;
                        flattened.insert(nebMesh);
                        q.push(nebMesh);
                    }
                }
            }

            std::cout << "finished flattening" << std::endl;
            // std::cout << "flattened " << flattened.size() << " meshes" << std::endl;
            // std::cout << this->V << std::endl;
            for (auto mesh: meshes) {
                mesh->updateVBOP();
            }
        }
        
        bool flattenMesh(Mesh* preMesh, Mesh* mesh, std::pair<int, int> edge, std::set<Mesh*> &flattened) {
            // find the remaining non-flattened vertex
            int fv1 = edge.first, fv2 = edge.second;
            int v3;
            for (int vid: mesh->vids) {
                if (vid != fv1 && vid != fv2) {
                    v3 = vid;
                    break;
                }
            }

            // flatten the remaining vertex v3 according to the flat position of v1 and v2
            Eigen::Vector3f fv3Pos;
            if (!flattenVertex(mesh, v3, fv1, fv2, preMesh->vid2fv[fv1], preMesh->vid2fv[fv2], fv3Pos, flattened))
                return false;

            // get flat v1 and flat v2 from pre Mesh
            mesh->vid2fv[fv1] = preMesh->vid2fv[fv1];
            mesh->vid2fv[fv2] = preMesh->vid2fv[fv2];
            mesh->vid2fv[v3] = fv3Pos;

            return true;
        }

        // compute the flat position of v3 according to the flat position of v1 and v2
        // check overlap
        bool flattenVertex(Mesh* mesh, int v3, int v1, int v2, Eigen::Vector3f fv1Pos, Eigen::Vector3f fv2Pos, Eigen::Vector3f &fv3Pos, std::set<Mesh*> &flattened) {
            Eigen::Vector3f flat1, flat2;
            Eigen::Vector3f v1Pos = mesh->vid2v[v1];
            Eigen::Vector3f v2Pos = mesh->vid2v[v2];
            Eigen::Vector3f v3Pos = mesh->vid2v[v3];
            float v1v2Len = (v2Pos - v1Pos).norm();
            float v1v3Len = (v3Pos - v1Pos).norm();
            float v2v3Len = (v3Pos - v2Pos).norm();

            // std::cout << "compute h" << std::endl;

            float S = (v2Pos-v1Pos).cross(v3Pos-v1Pos).norm()/2;
            float h = 2*S / v1v2Len;
            float b = sqrt(v1v3Len*v1v3Len - h*h);
            float c = sqrt(v2v3Len*v2v3Len - h*h);
            if (c > v1v2Len && c > b) b = -b;

            // std::cout << "h = " << h << std::endl;
            // std::cout << "compute fH and flat pos" << std::endl;
            
            // compute flat pos of H and flat pos of v3
            Eigen::Vector3f fH = ((v1v2Len-b)/v1v2Len)*fv1Pos + (b/v1v2Len)*fv2Pos;
            Eigen::Vector3f fv1v2 = fv2Pos - fv1Pos;
            Eigen::Vector3f flatDir = Eigen::Vector3f(-fv1v2.y(), fv1v2.x(), 0.).normalized();
            flat1 = fH + h * flatDir;
            flat2 = fH + h * (-flatDir);

            // std::cout << "fH = " << std::endl << fH << std::endl;
            // std::cout << "fv1v2 = " << std::endl << fv1v2 << std::endl;
            // std::cout << "flatDir = " << std::endl << flatDir << std::endl;

            // check overlap
            if (!overlap(flat1, fv1Pos, fv2Pos, flattened)) {
                // std::cout << "flat1 = " << std::endl << flat1 << std::endl;
                fv3Pos = flat1;
                return true;
            }
            else if (!overlap(flat2, fv1Pos, fv2Pos, flattened)) {
                // std::cout << "flat2 = " << std::endl << flat2 << std::endl;
                fv3Pos = flat2;
                return true;
            }
            // std::cout << "cannot flat" << std::endl;
            // std::cout << "flat1" << std::endl;
            // std::cout << flat1 << std::endl;
            // std::cout << "flat2" << std::endl;
            // std::cout << flat2 << std::endl;
            return false;
        }

        bool overlap(Eigen::Vector3f flatPos, Eigen::Vector3f fv1Pos, Eigen::Vector3f fv2Pos, std::set<Mesh*> &flattened) {
            // check if flatPos inside a flattened triangle mesh
            // std::cout << "check overlap" << std::endl;
            for (auto mesh: flattened) {
                int ov1, ov2, ov3;
                ov1 = mesh->vids[0]; ov2 = mesh->vids[1]; ov3 = mesh->vids[2];

                if (isInside(flatPos, mesh)) return true;
                // std::cout << "not inside" << std::endl;
                if (lineCross(flatPos, fv1Pos, mesh)) return true;
                // std::cout << "v1-v2 no lineCross" << std::endl;
                if (lineCross(flatPos, fv2Pos, mesh)) return true;
                // std::cout << "v1-v3 no lineCross" << std::endl;
                
            }

            return false;
        }
        bool lineCross(Eigen::Vector3f a, Eigen::Vector3f b, Mesh* mesh) {
            int ov1, ov2, ov3;
            ov1 = mesh->vids[0]; ov2 = mesh->vids[1]; ov3 = mesh->vids[2];
            if (lineCross(a, b, mesh->vid2fv[ov1], mesh->vid2fv[ov2])) return true;
            if (lineCross(a, b, mesh->vid2fv[ov2], mesh->vid2fv[ov3])) return true;
            if (lineCross(a, b, mesh->vid2fv[ov1], mesh->vid2fv[ov3])) return true;
            return false;
        }
        bool lineCross(Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c, Eigen::Vector3f d) {
            // parallel?
            Eigen::Vector3f AB = b-a;
            Eigen::Vector3f CD = d-c;
            if (AB.cross(CD).norm() == 0) return false;

            // get intersection
            Eigen::Matrix2f M;
            Eigen::Vector2f R;
            M << a.x()-b.x(), d.x()-c.x(), a.y()-b.y(),  d.y()-c.y();
            R << d.x()-b.x(), d.y()-b.y();
            Eigen::Vector2f x = M.colPivHouseholderQr().solve(R);

            // within range? 0 <= x <= 1
            return x(0) > 0 && x(0) < 1 && x(1) > 0 && x(1) < 1;
        }
        bool isInside(Eigen::Vector3f flatPos, Mesh *mesh) {
            int v1, v2, v3;
            v1 = mesh->vids[0]; v2 = mesh->vids[1]; v3 = mesh->vids[2];
            Eigen::Vector3f a, b, c;
            a = mesh->vid2fv[v1]; b = mesh->vid2fv[v2]; c = mesh->vid2fv[v3];

            Eigen::Matrix3f M;
            Eigen::Vector3f R;
            M << a(0),b(0),c(0),  a(1),b(1),c(1), 1,1,1;
            R << flatPos(0), flatPos(1), 1;
            Eigen::Vector3f x = M.colPivHouseholderQr().solve(R);

            return x(0) >= 0 && x(1) >= 0 && x(2) >= 0;
        }
};
class _3dObject {
    public:
        Eigen::MatrixXf box;
        Eigen::MatrixXf ModelMat;
        Eigen::MatrixXf ModelMat_T;
        Eigen::MatrixXf Adjust_Mat;
        Eigen::MatrixXf T_to_ori;
        Eigen::Vector4f barycenter;
        Eigen::VectorXi IDX;
        Eigen::MatrixXf V;
        Eigen::MatrixXf C;
        Eigen::MatrixXf Normals;

        int render_mode;
        double r, s, tx, ty;
        BezierCurve* bc;
        FlattenObject* flattenObj;

        std::vector<std::vector<Mesh*>> edges;
        std::vector<Mesh*> meshes;

        VertexArrayObject VAO;
        VertexBufferObject VBO_P;
        VertexBufferObject VBO_C;
        VertexBufferObject VBO_N;
        IndexBufferObject IBO_IDX;

        _3dObject(){}
        _3dObject(std::string off_path, int color_idx) {
            //load from off file
            Eigen::MatrixXf V, C;
            Eigen::VectorXi IDX;
            loadMeshfromOFF(off_path, V, C, IDX);
            C = Eigen::MatrixXf(3, V.cols());
            // int color_idx = rand() % colors.size();
            Eigen::Vector3i color = colors[color_idx];
            for (int i = 0; i < V.cols(); i++) {
                C.col(i) = color.cast<float>();
            }
            //compute the bouncing box
            box = get_bounding_box(V);
            //create class Mesh for each mech
            this->initial(V, C, IDX, box);
        }
        void initial(Eigen::MatrixXf V, Eigen::MatrixXf C, Eigen::VectorXi IDX, Eigen::MatrixXf bounding_box) {
            // make sure it is a point
            for (int i = 0; i < V.cols(); i++) {
                V.col(i)(3) = 1.0;
            }
            this->IDX = IDX;
            this->V = V;
            this->C = C;

            // Create a VAO
            this->VAO.init();
            this->VAO.bind();
            // Initialize the VBO with the vertices data
            this->VBO_P.init();
            this->VBO_P.update(V);
            this->VBO_C.init();
            this->VBO_C.update(C/255.0);
            this->VBO_N.init();
            this->IBO_IDX.init();
            this->IBO_IDX.update(IDX);

            this->ModelMat = Eigen::MatrixXf::Identity(4,4);
            this->ModelMat_T = Eigen::MatrixXf::Identity(4,4);
            this->T_to_ori = Eigen::MatrixXf::Identity(4,4);
            this->r = 0; this->s = 1; this->tx = 0; this->ty = 0;
            this->render_mode = 0;
            this->barycenter = (bounding_box.col(0) + bounding_box.col(1))/2.0;

            // Computer the translate Matrix from barycenter to the origin
            Eigen::Vector4f delta = Eigen::Vector4f(0.0, 0.0, 0.0, 1.0) - this->barycenter;
            T_to_ori.col(3)(0) = delta(0); T_to_ori.col(3)(1) = delta(1); T_to_ori.col(3)(2) = delta(2);

            // Adjust size
            this->initial_adjust(bounding_box);

            // initial edges
            for (int i = 0; i < V.cols(); i++) {
                this->edges.push_back(std::vector<Mesh*>());
            }
            std::cout << "start generating Meshs" << std::endl;
            for (int i = 0; i < IDX.rows(); i+=3) {
                int a = IDX(i), b = IDX(i+1), c = IDX(i+2);
                Eigen::MatrixXf Vblock(4, 3);
                Vblock << V.col(a), V.col(b), V.col(c);
                auto mesh = new Mesh(Vblock, bounding_box);
                this->meshes.push_back(mesh);
                this->edges[a].push_back(mesh);
                this->edges[b].push_back(mesh);
                this->edges[c].push_back(mesh);
            }

            // Compute normlas for each vertex
            if (this->edges.size() != V.cols()) {
                std::cout << "Assert failed" << std::endl;
                exit(1);
            }
            this->Normals.resize(V.rows(), V.cols());
            for (int i = 0; i < V.cols(); i++) {
                Eigen::Vector4f normal(0., 0., 0., 0.);
                for (Mesh* mesh: this->edges[i]) {
                    normal += mesh->normal;
                }
                this->Normals.col(i) = normal.normalized();
            }
            this->VBO_N.update(this->Normals);

            bc = new BezierCurve();

            std::cout << "meshes # = " << this->meshes.size() << std::endl;
            std::cout << "finish" << std::endl;

            // create flatten object
            this->flattenObj = new FlattenObject(V, IDX);
        }
        void initial_adjust(Eigen::MatrixXf bounding_box) {
            double maxx = bounding_box.col(1)(0), maxy = bounding_box.col(1)(1), maxz = bounding_box.col(1)(2);
            double minx = bounding_box.col(0)(0), miny = bounding_box.col(0)(1), minz = bounding_box.col(0)(2);
            double scale_factor = fmin(1.0/(maxx-minx), fmin(1.0/(maxy-miny), 1.0/(maxz-minz)));
            this->scale(scale_factor);
        }
        bool hit(Eigen::Vector4f ray_origin, Eigen::Vector4f ray_direction, float &dist) {

            bool intersected = false;
            int cnt = 0;
            for (auto mesh : this->meshes) {
                Eigen::Vector4f intersection;
                Eigen::MatrixXf mesh_V(4,3);
                int i = this->IDX(cnt*3), j = this->IDX(cnt*3+1), k = this->IDX(cnt*3+2);
                mesh_V << this->V.col(i), this->V.col(j), this->V.col(k);
                mesh_V = this->ModelMat*mesh_V;

                // the ray is parallel to the Mesh, no solution
                Eigen::Vector4f world_normal = ((this->ModelMat.inverse()).transpose()*mesh->normal).normalized();
                if (ray_direction.dot(world_normal) == 0) {
                    cnt++;
                    continue;
                }

                if (mesh->getIntersection(ray_origin, ray_direction, intersection, mesh_V)) {
                    intersected = true;
                    if (intersection == ray_origin) dist = 0;
                    else dist = std::fmin((intersection-ray_origin).norm(), dist);
                }
                cnt++;
            }
            return intersected;
        }
        void translate(Eigen::Vector4f delta) {
            // delta = this->Adjust_Mat.inverse()*delta;
            this->tx += delta(0); this->ty += delta(1);
            Eigen::MatrixXf T = Eigen::MatrixXf::Identity(4, 4);
            T.col(3)(0) = delta(0); T.col(3)(1) = delta(1); T.col(3)(2) = delta(2);
            Eigen::MatrixXf T_T = Eigen::MatrixXf::Identity(4, 4);
            T_T.col(3)(0) = -delta(0); T_T.col(3)(1) = -delta(1); T_T.col(3)(2) = -delta(2);
            this->update_Model_Mat(T, T_T, true);
        }
        void scale(double factor) {
            // double factor = 1+delta;
            this->s *= factor;
            Eigen::MatrixXf S = Eigen::MatrixXf::Identity(4, 4);
            S.col(0)(0) = factor; S.col(1)(1) = factor; S.col(2)(2) = factor;
            Eigen::MatrixXf S_T = Eigen::MatrixXf::Identity(4, 4);
            S_T.col(0)(0) = 1.0/factor; S_T.col(1)(1) = 1.0/factor; S_T.col(2)(2) = 1.0/factor;

            Eigen::MatrixXf I = Eigen::MatrixXf::Identity(4,4);
            S = (2*I-this->T_to_ori)*S*(this->T_to_ori);
            S_T = (2*I-this->T_to_ori)*S_T*(this->T_to_ori);

            this->update_Model_Mat(S, S_T, false);
        }
        void rotate(double degree, Eigen::Matrix4f rotateMat) {
            double r = degree*PI/180.0;
            this->r += r;
            Eigen::MatrixXf R = Eigen::MatrixXf::Identity(4, 4);
            R.col(0)(0) = std::cos(r); R.col(0)(1) = std::sin(r);
            R.col(1)(0) = -std::sin(r); R.col(1)(1) = std::cos(r);
            Eigen::MatrixXf R_T = Eigen::MatrixXf::Identity(4, 4);
            R_T.col(0)(0) = std::cos(r); R_T.col(0)(1) = -std::sin(r);
            R_T.col(1)(0) = std::sin(r); R_T.col(1)(1) = std::cos(r);

            Eigen::MatrixXf I = Eigen::MatrixXf::Identity(4,4);
            R = (2*I-this->T_to_ori)*rotateMat*(this->T_to_ori);
            R_T = (2*I-this->T_to_ori)*rotateMat.inverse()*(this->T_to_ori);
            // R = (2*I-this->T_to_ori)*R*(this->T_to_ori);
            // R_T = (2*I-this->T_to_ori)*R_T*(this->T_to_ori);

            this->update_Model_Mat(R, R_T, false);
        }
        void update_Model_Mat(Eigen::MatrixXf M, Eigen::MatrixXf M_T, bool left) {
            if (left) {
                // left cross mul
                this->ModelMat = M*this->ModelMat;
                this->ModelMat_T = this->ModelMat_T*M_T;
            }
            else {
                // right cross mul
                this->ModelMat = this->ModelMat*M;
                this->ModelMat_T = M_T*this->ModelMat_T;
            }
        }
};
class Cube: public _3dObject {
    public:
        Eigen::Vector4f center;
        Cube(int color_idx, double x, double y, double z, double len = 1.0):_3dObject() {
            std::cout << "enter initialization" << std::endl;
            this->center = Eigen::Vector4f(x, y, z, 1);
            Eigen::Vector4f HB = Eigen::Vector4f(-len, len, len, 0);
            Eigen::Vector4f EC = Eigen::Vector4f(len, len, len, 0);
            Eigen::Vector4f GA = Eigen::Vector4f(-len, len, -len, 0);
            Eigen::Vector4f FD = Eigen::Vector4f(len, len, -len, 0);
            Eigen::Vector4f A = center+0.5*GA, G = center-0.5*GA;
            Eigen::Vector4f B = center+0.5*HB, H = center-0.5*HB;
            Eigen::Vector4f C = center+0.5*EC, E = center-0.5*EC;
            Eigen::Vector4f D = center+0.5*FD, F = center-0.5*FD;
            // 6 faces, 12 Meshs
            Eigen::MatrixXf V(4, 8);
            V << A, B, C, D, E, F, G, H;
            Eigen::VectorXi IDX(36);
            IDX << 0,1,2, 2,3,0, 4,6,5, 6,4,7, 0,5,1, 5,0,4, 1,6,2, 1,5,6, 3,2,6, 6,7,3, 3,7,0, 7,4,0;
            Eigen::MatrixXf Color(3, 8);
            Eigen::Vector3i color = colors[color_idx];
            for (int i = 0; i < 8; i++) {
                Color.col(i) = color.cast<float>();
            }
            std::cout << "ready to generate Meshs" << std::endl;
            // bounding box
            Eigen::MatrixXf box(4, 2);
            box << x-len/2.0, x+len/2.0, y-len/2.0, y+len/2.0, z-len/2.0, z+len/2.0, 1, 1;
            this->initial(V, Color, IDX, box);
        }
};
class _3dObjectBuffer {
    public:
        std::vector<_3dObject*> _3d_objs;
        _3dObject* selected_obj;
        RayTracer* ray_tracer;
        int color_idx;

        _3dObjectBuffer() {
            selected_obj = nullptr;
            ray_tracer = new RayTracer();
            ray_tracer->add_light(Eigen::Vector4f(0.0, 0.0, 1, 1.0));
            color_idx = 0;
        }
        void add_cube(double x, double y, double z, double len=1.0) {
            this->color_idx = (this->color_idx)%colors.size();
            this->color_idx++;
            _3d_objs.push_back(new Cube(this->color_idx, x, y, z, len));
        }
        void add_object(std::string off_path) {
            this->color_idx = (this->color_idx)%colors.size();
            this->color_idx++;
            _3d_objs.push_back(new _3dObject(off_path, this->color_idx));
        }
        bool hit(Eigen::Vector4f ray_origin, Eigen::Vector4f ray_direction, float &ret_dist, int mode=CILCK_ACTION) {
            // find the hit one if any
            float min_dist = DIST_MAX;
            _3dObject* selected = nullptr;
            for (auto obj: _3d_objs) {
                float dist = DIST_MAX;
                if (obj->hit(ray_origin, ray_direction, dist)) {
                    if (selected == nullptr || min_dist > dist) {
                        min_dist = dist;
                        selected = obj;
                    }
                }
            }
            ret_dist = min_dist;

            if (mode == CILCK_ACTION)
                this->selected_obj = selected;

            return selected != nullptr;
        }
        bool translate(Eigen::Vector4f delta) {
            if (this->selected_obj == nullptr) return false;
            this->selected_obj->translate(delta);
            return true;
        }
        bool rotate(double degree, Eigen::Vector3f rotateAixs) {
            if (this->selected_obj == nullptr) return false;
            auto M = this->get_rotate_mat(degree, rotateAixs);
            this->selected_obj->rotate(degree, M);
            return true;
        }
        bool scale(double factor) {
            if (this->selected_obj == nullptr) return false;
            this->selected_obj->scale(factor);
            return true;
        }
        bool switch_render_mode(int mode) {
            if (this->selected_obj == nullptr) return false;
            this->selected_obj->render_mode = mode;
            return true;
        }
        bool delete_obj() {
            if (this->selected_obj != nullptr) {
                this->_3d_objs.erase(std::remove(this->_3d_objs.begin(), this->_3d_objs.end(), this->selected_obj), this->_3d_objs.end());
                delete this->selected_obj;
                this->selected_obj = nullptr;
                return true;
            }
            return false;
        }
        Eigen::Matrix4f get_rotate_mat(float angle, Eigen::Vector3f rotateAixs) {
            float r = (PI/180) * (angle/2);
            float x = rotateAixs.x() * sin(r);
            float y = rotateAixs.y() * sin(r);
            float z = rotateAixs.z() * sin(r);
            float w = cos(r);
            Eigen::Matrix4f rotate_mat;
            rotate_mat << 
            1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w, 0,
            2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*x-2*x*w, 0,
            2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y, 0,
            0,0,0,1;
            return rotate_mat;
        }
};

_3dObjectBuffer* _3d_objs_buffer;
std::vector<_3dObject*>* _3d_objs;
Camera *camera;
std::chrono::high_resolution_clock::time_point t_pre;
bool is_playing = false;
int playing_cnt = 0;
double unitTime = 0.;
Eigen::MatrixXf AnimeT;

void update_window_scale(GLFWwindow* window) {
    camera->update_project_mat(window);
    camera->look_at(window);
}

Eigen::Vector4f get_click_position(GLFWwindow* window) {
    // Get the position of the mouse in the window
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Get the size of the window
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Convert screen position to world coordinates
    Eigen::Vector4f p_screen(xpos,height-1-ypos,0.0,1.0);
    Eigen::Vector4f p_canonical((p_screen[0]/width)*2-1,(p_screen[1]/height)*2-1,-camera->n,1.0);
    Eigen::Vector4f p_camera = camera->get_project_mat().inverse() * p_canonical;
    if (fabs(p_camera(3)-1.0) > 0.001) {
        p_camera = p_camera/p_camera(3);
    }
    Eigen::Vector4f p_world = camera->ViewMat.inverse() * p_camera;

    return p_world;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Convert screen position to world coordinates
    Eigen::Vector4f click_point = get_click_position(window);

    // Update the position of the first vertex if the left button is pressed
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        Eigen::Vector4f ray_origin = click_point;
        // orth projection
        Eigen::Vector4f ray_direction;
        if (camera->project_mode == ORTHO) {
            ray_direction = -to_4_vec(camera->forward);
            std::cout << "ORTHO ray_direction" << std::endl;
            std::cout << ray_direction << std::endl;
        }
        else if (camera->project_mode == PERSPECT) {
            ray_direction = (click_point-to_4_vec(camera->position)).normalized();
            ray_direction(3) = 0.0;
            std::cout << "PERSPECT ray_direction" << std::endl;
            std::cout << ray_direction << std::endl;
        }
        float dist = 0;
        if (_3d_objs_buffer->hit(ray_origin, ray_direction, dist)) {
            drag = true;
            hit_dist = dist;
            pre_cursor_point = click_point;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        drag = false;
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Convert screen position to world coordinates
    Eigen::Vector4f click_point = get_click_position(window);

    if (drag) {
        Eigen::Vector4f delta = click_point-pre_cursor_point;
        _3d_objs_buffer->translate(delta);
        pre_cursor_point = click_point;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    switch (key)
    {
        // Add a cube
        case  GLFW_KEY_1:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "add a unit cube");
                _3d_objs_buffer->add_cube(0, 0, 0);
            }
            break;
        // Add a Bumpy
        case  GLFW_KEY_2:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "add a bumpy");
                _3d_objs_buffer->add_object(BUMPY_CUBE_OFF_PATH);
            }
            break;
        // Add a Bunny
        case  GLFW_KEY_3:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "add a bunny");
                _3d_objs_buffer->add_object(BUNNY_OFF_PATH);
            }
            break;
        // Add a Cube from OFF
        case  GLFW_KEY_4:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "add a cube");
                _3d_objs_buffer->add_object(CUBE_OFF_PATH);
            }
            break;
        // Add a Cone
        case  GLFW_KEY_5:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "add a cone");
                _3d_objs_buffer->add_object(CONE_OFF_PATH);
            }
            break;
         // Deleta an object
        case  GLFW_KEY_DELETE:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->delete_obj()) {
                    glfwSetWindowTitle (window, "deleted one object");
                }
            }
            break;
        // Clockwise rotate 10 degree
        case  GLFW_KEY_H:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->rotate(10, camera->forward)) {
                    glfwSetWindowTitle (window, "clockwise rotate 10");
                }
            }
            break;
        // Counter-clockwise rotate 10 degree
        case  GLFW_KEY_J:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->rotate(-10, camera->forward)) {
                    glfwSetWindowTitle (window, "counter-clockwise rotate 10");
                }
            }
            break;
        // Scale up 25%
        case GLFW_KEY_K:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->scale(1.25)) {
                    glfwSetWindowTitle (window, "scale up 25%");
                }
            }
            break;
        // Scale down 25%
        case GLFW_KEY_L:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->scale(1.0/1.25)) {
                    glfwSetWindowTitle (window, "scale down 25%");
                }
            }
            break;
        // move object up (camera up direction)
        case GLFW_KEY_W:
            if (action == GLFW_PRESS) {
                // if (_3d_objs_buffer->translate(Eigen::Vector4f(0., 0.2, 0., 0.))) {
                if (_3d_objs_buffer->translate(to_4_vec(camera->up)/10.0)) {
                    glfwSetWindowTitle (window, "move up");
                }
            }
            break;
        // move object down (camera -up direction)
        case GLFW_KEY_S:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->translate(-to_4_vec(camera->up)/10.0)) {
                    glfwSetWindowTitle (window, "move down");
                }
            }
            break;
        // move object left (camera -right direction)
        case GLFW_KEY_A:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->translate(-to_4_vec(camera->right)/10.0)) {
                    glfwSetWindowTitle (window, "move left");
                }
            }
            break;
        // move object right (camera right direction)
        case GLFW_KEY_D:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->translate(to_4_vec(camera->right)/10.0)) {
                    glfwSetWindowTitle (window, "move right");
                }
            }
            break;
        // move object toward camera (camera -forward direction)
        case GLFW_KEY_E:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->translate(-to_4_vec(camera->forward)/10.0)) {
                    glfwSetWindowTitle (window, "move toward camera");
                }
            }
            break;
        // move object away from camera (camera forward direction)
        case GLFW_KEY_Q:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->translate(to_4_vec(camera->forward)/10.0)) {
                    glfwSetWindowTitle (window, "move away from camera");
                }
            }
            break;
        // Switch render mode
        case GLFW_KEY_I:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->switch_render_mode(WIREFRAME)) {
                    glfwSetWindowTitle (window, "switch render mode to Wire Frame");
                }
            }
            break;
        case GLFW_KEY_O:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->switch_render_mode(FLAT_SHADING)) {
                    glfwSetWindowTitle (window, "switch render mode to Flat Shading");
                }
            }
            break;
        case GLFW_KEY_P:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->switch_render_mode(PHONG_SHADING)) {
                    glfwSetWindowTitle (window, "switch render mode to Phong Shading");
                }
            }
            break;
        // Move the camera
        case GLFW_KEY_LEFT:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "rotate camera to left 10 degree");
                camera->rotate(0, -10);
                camera->look_at(window);
            }
            break;
        case GLFW_KEY_RIGHT:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "rotate camera to right 10 degree");
                camera->rotate(0, 10);
                camera->look_at(window);
            }
            break;
        case GLFW_KEY_UP:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "rotate camera to up 10 degree");
                camera->rotate(10, 0);
                camera->look_at(window);
            }
            break;
        case GLFW_KEY_DOWN:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "rotate camera to down 10 degree");
                camera->rotate(-10, 0);
                camera->look_at(window);
            }
            break;
        // zoom in / zoom out
        case GLFW_KEY_EQUAL:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "zoom in 10%");
                camera->zoom(0.9);
                camera->look_at(window);
            }
            break;
        case GLFW_KEY_MINUS:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "zoom out 10%");
                camera->zoom(1.0/0.9);
                camera->look_at(window);
            }
            break;
        // reset camera
        case GLFW_KEY_R:
            if (action == GLFW_PRESS) {
                glfwSetWindowTitle (window, "reset camera");
                camera->reset();
                camera->look_at(window);
            }
            break;
        // switch project mode
        case GLFW_KEY_M:
            if (action == GLFW_PRESS) {
                camera->switch_project_mode();
                if (camera->project_mode == ORTHO)
                    glfwSetWindowTitle (window, "switch to ortho projection");
                else
                    glfwSetWindowTitle (window, "switch to perspective projection");
            }
            break;
        // bezier curve
        case GLFW_KEY_C:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    glfwSetWindowTitle (window, "add a control point");
                    _3d_objs_buffer->selected_obj->bc->insert_point(0., 0., 0.);
                }
            }
            break;
        case GLFW_KEY_COMMA:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(-0.2, 0., 0., 0.), bc->V.cols()-1))
                        glfwSetWindowTitle (window, "move a control point to left");
                }
            }
            break;
        case GLFW_KEY_PERIOD:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(0.2, 0., 0., 0.), bc->V.cols()-1))
                        glfwSetWindowTitle (window, "move a control point to right");
                }
            }
            break;
        case GLFW_KEY_APOSTROPHE:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(0., 0.2, 0., 0.), bc->V.cols()-1))
                        glfwSetWindowTitle (window, "move a control point to up");
                }
            }
            break;
        case GLFW_KEY_SLASH:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(0., -0.2, 0., 0.), bc->V.cols()-1))
                        glfwSetWindowTitle(window, "move a control point to down");
                }
            }
            break;
        case GLFW_KEY_LEFT_BRACKET:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(0., 0., -0.2, 0.), bc->V.cols()-1))
                        glfwSetWindowTitle(window, "move a control point to front");
                }
            }
            break;
        case GLFW_KEY_RIGHT_BRACKET:
            if (action == GLFW_PRESS) {
                if (_3d_objs_buffer->selected_obj != nullptr) {
                    auto bc = _3d_objs_buffer->selected_obj->bc;
                    if (bc->translate(Eigen::Vector4f(0., 0., 0.2, 0.), bc->V.cols()-1))
                        glfwSetWindowTitle(window, "move a control point to back");
                }
            }
            break;
        // play animation
        case GLFW_KEY_SPACE:
            if (action == GLFW_PRESS) {
                if (playing_cnt == 0) {
                    glfwSetWindowTitle(window, "playing animation...");
                    playing_cnt = _3d_objs_buffer->_3d_objs.size();
                    int mxlen = 0;
                    for (auto obj: _3d_objs_buffer->_3d_objs) {
                        obj->bc->nxt_curve_pt = 0;
                        obj->bc->anime_finish = false;
                        if (obj->bc->curve_points.cols() > mxlen) mxlen = obj->bc->curve_points.cols();
                    }
                    if (mxlen > 0) {
                        unitTime = 5.0/mxlen;
                    }
                    auto t_pre = std::chrono::high_resolution_clock::now();
                    std::cout << "start play" << std::endl;
                }
            }
            break;
        default:
            break;
    }
}

int main(void)
{
    GLFWwindow* window;

    // Initialize the library
    if (!glfwInit())
        return -1;

    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);

    // Ensure that we get at least a 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(640, 480, "3D Editor", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    #ifndef __APPLE__
      glewExperimental = true;
      GLenum err = glewInit();
      if(GLEW_OK != err)
      {
        /* Problem: glewInit failed, something is seriously wrong. */
       fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
      }
      glGetError(); // pull and savely ignonre unhandled errors like GL_INVALID_ENUM
      fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
    #endif

    int major, minor, rev;
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    rev = glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
    printf("OpenGL version recieved: %d.%d.%d\n", major, minor, rev);
    printf("Supported OpenGL is %s\n", (const char*)glGetString(GL_VERSION));
    printf("Supported GLSL is %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize the OpenGL Program
    // A program controls the OpenGL pipeline and it must contains
    // at least a vertex shader and a fragment shader to be valid
    Program program;
    const GLchar* vertex_shader =
            "#version 150 core\n"
                    "in vec4 position;"
                    "uniform vec3 color;"
                    "uniform mat4 ViewMat;"
                    "uniform mat4 ModelMat;"
                    "uniform mat4 ProjectMat;"
                    "out vec3 f_color;"
                    "void main()"
                    "{"
                    "    gl_Position = ProjectMat*ViewMat*ModelMat*position;"
                    "    f_color = color;"
                    "}";
    const GLchar* fragment_shader =
            "#version 150 core\n"
                    "in vec3 f_color;"
                    "out vec4 outColor;"
                    "uniform vec3 lineColor;"
                    "uniform int isLine;"
                    "void main()"
                    "{"
                    "    if (isLine == 1)"
                    "       outColor = vec4(lineColor, 1.0);"
                    "    else"
                    "       outColor = vec4(f_color, 1.0);"
                    "}";

    // Compile the two shaders and upload the binary to the GPU
    // Note that we have to explicitly specify that the output "slot" called outColor
    // is the one that we want in the fragment buffer (and thus on screen)
    program.init(vertex_shader,fragment_shader,"outColor");
    program.bind();

    // Register the keyboard callback
    glfwSetKeyCallback(window, key_callback);

    // Register the mouse callback
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Register the mouse cursor position callback
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Update viewport
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Inital control objects
    _3d_objs_buffer = new _3dObjectBuffer();
    camera = new Camera(window);

    // bind light
    auto light = _3d_objs_buffer->ray_tracer->lights[0];
    glUniform4fv(program.uniform("light.position"), 1, light->position.data());
    glUniform3fv(program.uniform("light.intensities"), 1, light->intensity.data());
    // bind special colors
    Eigen::Vector3f special_color = (SELCET_COLOR.cast<float>())/255.0;
    // glUniform3fv(program.uniform("selectedColor"), 1, special_color.data());
    // special_color = (BLACK.cast<float>())/255.0;
    special_color = (WHITE.cast<float>())/255.0;
    glUniform3fv(program.uniform("lineColor"), 1, special_color.data());

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Bind your program
        program.bind();

        // Clear the framebuffer
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Enable depth test
        glEnable(GL_DEPTH_TEST);

        glEnable(GL_PROGRAM_POINT_SIZE);

        // Update window scaling
        update_window_scale(window);
        // Send the View Mat to Vertex Shader
        Eigen::Vector4f tmp;
        tmp(0) = camera->position(0); tmp(1) = camera->position(1); tmp(2) = camera->position(2);
        tmp(3) = 1.0;
        glUniform4fv(program.uniform("viewPosition"), 1, tmp.data());
        glUniformMatrix4fv(program.uniform("ViewMat"), 1, GL_FALSE, camera->ViewMat.data());
        glUniformMatrix4fv(program.uniform("ProjectMat"), 1, GL_FALSE, camera->get_project_mat().data());

        glUniform1i(program.uniform("isSelected"), 1);
        glUniform1i(program.uniform("isFlatShading"), 0);
        for (auto obj: _3d_objs_buffer->_3d_objs) {
            // prepare
            auto flatObj = obj->flattenObj;

            for (auto mesh: flatObj->meshes) {
                mesh->VAO.bind();
                program.bindVertexAttribArray("position", mesh->VBO_P);
                glUniformMatrix4fv(program.uniform("ModelMat"), 1, GL_FALSE, obj->ModelMat.data());
                glUniform3fv(program.uniform("color"), 1, mesh->color.data());

                glUniform1i(program.uniform("isLine"), 1);
                glDrawArrays(GL_LINE_LOOP, 0, 3);
                glUniform1i(program.uniform("isLine"), 0);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
            // // obj->VAO.bind();
            // flat->VAO.bind();
            // program.bindVertexAttribArray("position",flat->VBO_P);
            // // program.bindVertexAttribArray("position",obj->VBO_P);
            // // program.bindVertexAttribArray("color",obj->VBO_C);
            // // program.bindVertexAttribArray("normal",obj->VBO_N);
            // // Eigen::MatrixXf ModelMat = Eigen::MatrixXf::Identity(4, 4);
            // // glUniformMatrix4fv(program.uniform("ModelMat"), 1, GL_FALSE, ModelMat.data());
            // glUniformMatrix4fv(program.uniform("ModelMat"), 1, GL_FALSE, obj->ModelMat.data());

            // glUniform1i(program.uniform("isSelected"), 1);
            // glUniform1i(program.uniform("isLine"), 0);
            // glUniform1i(program.uniform("isFlatShading"), 0);
            
            // // glDrawArrays(GL_TRIANGLES, 0, flat->flatV.cols());
            // // glDrawElements(GL_TRIANGLES, flat->IDX.rows(), GL_UNSIGNED_INT, (void*)0);
            // for (int i = 0; i < flat->F.cols(); i++) {
            //     glUniform1i(program.uniform("isLine"), 1);
            //     glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, (void*)(sizeof(int)* (i*3)));
            //     glUniform1i(program.uniform("isLine"), 0);
            //     glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void*)(sizeof(int)* (i*3)));
            // }
        }

        // for (auto obj: _3d_objs_buffer->_3d_objs) {
        //     // prepare
        //     obj->VAO.bind();
        //     program.bindVertexAttribArray("position",obj->VBO_P);
        //     program.bindVertexAttribArray("color",obj->VBO_C);
        //     program.bindVertexAttribArray("normal",obj->VBO_N);
        //     glUniformMatrix4fv(program.uniform("ModelMat"), 1, GL_FALSE, obj->ModelMat.data());

        //     glUniform1i(program.uniform("isLine"), 0);
        //     glUniform1i(program.uniform("isFlatShading"), 0);

        //     // Anime
        //     if (!obj->bc->anime_finish) {
        //         auto t_now = std::chrono::high_resolution_clock::now();
        //         double duration = std::chrono::duration_cast<std::chrono::duration<double>>(t_now-t_pre).count();
        //         t_pre = t_now;
        //         auto bc = obj->bc;
        //         if (bc->nxt_curve_pt < bc->curve_points.cols()) {
        //             Eigen::Vector4f delta = bc->curve_points.col(bc->nxt_curve_pt++) - obj->ModelMat*obj->barycenter;
        //             AnimeT = Eigen::MatrixXf::Identity(4, 4);
        //             AnimeT.col(3)(0) = delta(0); AnimeT.col(3)(1) = delta(1); AnimeT.col(3)(2) = delta(2);
        //             glUniformMatrix4fv(program.uniform("AnimeT"), 1, GL_FALSE, AnimeT.data());
        //             glUniform1i(program.uniform("isAnime"), 1);
        //         }
        //         else {
        //             playing_cnt -= 1;
        //             if (playing_cnt == 0) {
        //                 glUniform1i(program.uniform("isAnime"), 0);
        //                 glfwSetWindowTitle (window, "animation finished");
        //             }
        //             obj->bc->anime_finish = true;
        //         }
        //     }

        //     if (obj == _3d_objs_buffer->selected_obj)
        //         glUniform1i(program.uniform("isSelected"), 1);
        //     else
        //         glUniform1i(program.uniform("isSelected"), 0);

        //     if (obj->render_mode == WIREFRAME) {
        //         glUniform1i(program.uniform("isLine"), 1);
        //         for (int i = 0; i < obj->meshes.size(); i++) {
        //             glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, (void*)(sizeof(int)* (i*3)));
        //         }
        //     }
        //     else if (obj->render_mode == FLAT_SHADING) {
        //         glUniform1i(program.uniform("isFlatShading"), 1);
        //         for (int i = 0; i < obj->meshes.size(); i++) {
        //             glUniform4fv(program.uniform("meshNormal"), 1, obj->meshes[i]->normal.data());
                    
        //             glUniform1i(program.uniform("isLine"), 1);
        //             glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, (void*)(sizeof(int)* (i*3)));
        //             glUniform1i(program.uniform("isLine"), 0);
        //             glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void*)(sizeof(int)* (i*3)));
        //         }
        //     }
        //     else if (obj->render_mode == PHONG_SHADING) {
        //         glDrawElements(GL_TRIANGLES, obj->IDX.rows(), GL_UNSIGNED_INT, (void*)0);
        //     }

        //     // cancel Anime Translate for the curve
        //     glUniform1i(program.uniform("isAnime"), 0);

        //     if (_3d_objs_buffer->selected_obj == obj) {
        //         glUniform1i(program.uniform("isBezierCurve"), 1);
        //         // Draw control points if in bezier curve mode
        //         if (obj->bc->V.cols() > 0) {
        //             // Draw lines between control points
        //             obj->bc->VAO.bind();
        //             program.bindVertexAttribArray("position",obj->bc->VBO_P);
        //             for (int i = 0; i < obj->bc->V.cols()-1; i++) {
        //                 glDrawArrays(GL_LINES, i, 2);
        //             }
        //             // Draw control points
        //             program.bindVertexAttribArray("position",obj->bc->VBO_P);
        //             glDrawArrays(GL_POINTS, 0, obj->bc->V.cols());
        //             // Draw the curve
        //             obj->bc->VAO_C.bind();
        //             program.bindVertexAttribArray("position",obj->bc->VBO_CP);
        //             for (int i = 0; i < obj->bc->curve_points.cols()-1; i++) {
        //                 glDrawArrays(GL_LINES, i, 2);
        //             }
        //         }
        //         glUniform1i(program.uniform("isBezierCurve"), 0);
        //     }
        // }

        // Swap front and back buffers
        glfwSwapBuffers(window);

        if (playing_cnt > 0) continue;
        
        // Poll for and process events
        glfwPollEvents();
    }

    // Deallocate opengl memory
    program.free();

    // Deallocate glfw internals
    glfwTerminate();
    return 0;
}