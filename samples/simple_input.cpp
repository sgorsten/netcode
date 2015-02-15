#include <netcode.h>
#include <GLFW\glfw3.h>
#include <iostream>

int main() try
{
    // Initialize protocol
    auto protocol = ncCreateProtocol(30);
    auto characterCl = ncCreateClass(protocol);
    auto characterPosX = ncCreateInt(characterCl);
    auto characterPosY = ncCreateInt(characterCl);
    auto inputCl = ncCreateClass(protocol);
    auto inputTargetX = ncCreateInt(inputCl);
    auto inputTargetY = ncCreateInt(inputCl);

    // Initialize server
    auto serverAuth = ncCreateAuthority(protocol);
    auto playerChar = ncCreateObject(serverAuth, characterCl);
    auto serverPeer = ncCreatePeer(serverAuth);
    ncSetVisibility(serverPeer, playerChar, 1);

    // Initialize client
    auto clientAuth = ncCreateAuthority(protocol);
    auto playerInput = ncCreateObject(clientAuth, inputCl);
    auto clientPeer = ncCreatePeer(clientAuth);
    ncSetVisibility(clientPeer, playerInput, 1);

    if(glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit(...) failed.");
    auto win = glfwCreateWindow(640, 480, "Simple Input", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    while(!glfwWindowShouldClose(win))
    {
        {
            for(int i=0, n=ncGetViewCount(serverPeer); i<n; ++i)
            {
                auto view = ncGetView(serverPeer, i);
                if(ncGetViewClass(view) == inputCl)
                {
                    int x = ncGetViewInt(view, inputTargetX);
                    int y = ncGetViewInt(view, inputTargetY);

                    ncSetObjectInt(playerChar, characterPosX, x);
                    ncSetObjectInt(playerChar, characterPosY, y);
                }
            }

            //ncSetObjectInt(playerChar, characterPosX, 320);
            //ncSetObjectInt(playerChar, characterPosY, 240);
            ncPublishFrame(serverAuth);

            auto message = ncProduceMessage(serverPeer);
            ncConsumeMessage(clientPeer, ncGetBlobData(message), ncGetBlobSize(message));
            ncFreeBlob(message);
        }

        {
            // Render client-side views of server-side objects
            glClear(GL_COLOR_BUFFER_BIT);
            glPushMatrix();
            glOrtho(0, 640, 480, 0, -1, +1);
            for(int i=0, n=ncGetViewCount(clientPeer); i<n; ++i)
            {
                auto view = ncGetView(clientPeer, i);
                if(ncGetViewClass(view) == characterCl)
                {
                    int x = ncGetViewInt(view, characterPosX);
                    int y = ncGetViewInt(view, characterPosY);
                    glBegin(GL_TRIANGLE_FAN);
                    for(int j=0; j<12; ++j)
                    {
                        float a = 6.28f * j / 12;
                        glVertex2f(x + cosf(a)*10, y + sinf(a)*10);
                    }
                    glEnd();
                }
            }
            glPopMatrix();
            glfwSwapBuffers(win);

            // Retrieve input and publish it in client-side objects
            glfwPollEvents();
            if(glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            {
                double x, y;
                glfwGetCursorPos(win, &x, &y);
                ncSetObjectInt(playerInput, inputTargetX, static_cast<int>(x));
                ncSetObjectInt(playerInput, inputTargetY, static_cast<int>(y));
            }
            ncPublishFrame(clientAuth);

            // Send a message to the server
            auto message = ncProduceMessage(clientPeer);
            ncConsumeMessage(serverPeer, ncGetBlobData(message), ncGetBlobSize(message));
            ncFreeBlob(message);            
        }
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << "Caught exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}