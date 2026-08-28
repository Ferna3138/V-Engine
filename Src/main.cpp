#include "First_App.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "Renderer/GLTF_Loader.hpp"

int main(){
    //GLTF_Smoke smoke;
    //smoke.GLTF_SmokeTest("Models/GLTF/Duck/glTF/Duck.gltf");
    FirstApp app{};
    
    
    try{
        app.run();
    }catch(const std::exception &e){
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}