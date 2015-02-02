#include "View.h"

#include <cstdint>
#include <vector>
#include <iostream>
#include <random>
#include <GLFW/glfw3.h>

class Server
{
    struct PhysicsObject { float px, py, vx, vy; VObject vobj; };

	VServer server;
    VPeer peer1, peer2;
    std::vector<PhysicsObject> objects;
	VObject bar;
	float bp, bv;
public:
	Server()
	{
		const VClass classes[] = {vCreateClass(2), vCreateClass(4)};
		server = vCreateServer(classes, 2, 30);
        peer1 = vCreatePeer(server);
        peer2 = vCreatePeer(server);

		std::mt19937 engine;
        objects.resize(50);
		for (auto & object : objects)
		{
			object.px = std::uniform_real_distribution<float>(50, 1230)(engine);
			object.py = std::uniform_real_distribution<float>(50, 670)(engine);
			object.vx = std::uniform_real_distribution<float>(-100, +100)(engine);
			object.vy = std::uniform_real_distribution<float>(-100, +100)(engine);
			object.vobj = vCreateObject(server, classes[0]);
		}
		bar = vCreateObject(server, classes[1]);
        vSetVisibility(peer1, bar, 1);
		bp = 100;
		bv = -25;
	}

	void Update(float timestep)
	{
		for (auto & object : objects)
		{
            if(!object.vobj) continue;

			object.px += object.vx * timestep;
			object.py += object.vy * timestep;
			if (object.px < 20 && object.vx < 0) object.vx = -object.vx;
			if (object.px > 1260 && object.vx > 0) object.vx = -object.vx;
			if (object.py < 20 && object.vy < 0) object.vy = -object.vy;
			if (object.py > 700 && object.vy > 0) object.vy = -object.vy;

			vSetObjectInt(object.vobj, 0, object.px * 10);
			vSetObjectInt(object.vobj, 1, object.py * 10);
            vSetVisibility(peer1, object.vobj, object.px < 800 && object.py < 500);
            vSetVisibility(peer2, object.vobj, object.px > 480 && object.py > 220);
		}

		bp += bv * timestep;
		if (bp < 10 && bv < 0)
		{
			bp = 10;
			bv = -bv;
		}
		if (bp > 100 && bv > 0)
		{
			bp = 100;
			bv = -bv;
		}

		vSetObjectInt(bar, 0, bp);
		vSetObjectInt(bar, 1, (100 - bp) * 255 / 100);
		vSetObjectInt(bar, 2, bp * 255 / 100);
		vSetObjectInt(bar, 3, 32);

        vPublishFrame(server);
	}

    size_t GetMemUsage() const { return vDebugServerMemoryUsage(server); }

    void Draw() const
    {
        glColor3f(0.3f, 0.3f, 0.3f);
        for(auto & obj : objects)
        {
            if(!obj.vobj) continue;
		    glBegin(GL_TRIANGLE_FAN);	    
		    for (int i = 0; i < 12; ++i)
		    {
			    float a = i*6.28f / 12;
			    glVertex2f(obj.px + cos(a) * 10, obj.py + sin(a) * 10);
		    }
		    glEnd();
        }
    }

	std::vector<uint8_t> ProduceUpdate(int peer)
	{
        auto blob = vProduceUpdate(peer ? peer2 : peer1);
        std::vector<uint8_t> buffer(vGetBlobSize(blob));
        memcpy(buffer.data(), vGetBlobData(blob), buffer.size());
        vFreeBlob(blob);
        return buffer;
	}

    void ConsumeResponse(int peer, const std::vector<uint8_t> & buffer)
    {
        vConsumeResponse(peer ? peer2 : peer1, buffer.data(), buffer.size());
    }
};

class Client
{
	VClass objectClass, barClass;
	VClient client;
public:
	Client()
	{
		objectClass = vCreateClass(2);
		barClass = vCreateClass(4);
		const VClass classes [] = { objectClass, barClass };
		client = vCreateClient(classes, 2, 30);
	}

    size_t GetMemUsage() const { return vDebugClientMemoryUsage(client); }

	void Draw(float r, float g, float b) const
	{
		for (int i = 0, n = vGetViewCount(client); i < n; ++i)
		{
			auto view = vGetView(client, i);
			if (vGetViewClass(view) == objectClass)
			{
				float x = vGetViewInt(view, 0)*0.1f;
				float y = vGetViewInt(view, 1)*0.1f;
				glBegin(GL_TRIANGLE_FAN);
				glColor3f(r, g, b);
				for (int i = 0; i < 12; ++i)
				{
					float a = i*6.28f / 12;
					glVertex2f(x + cos(a) * 10, y + sin(a) * 10);
				}
				glEnd();
			}
			if (vGetViewClass(view) == barClass)
			{
				int p = vGetViewInt(view, 0);
				int r = vGetViewInt(view, 1);
				int g = vGetViewInt(view, 2);
				int b = vGetViewInt(view, 3);
				glBegin(GL_QUADS);
				glColor3ub(r, g, b);
				glVertex2i(10, 10);
				glVertex2i(10 + p, 10);
				glVertex2i(10 + p, 20);
				glVertex2i(10, 20);
				glEnd();
			}
		}
	}

	std::vector<uint8_t> Update(const std::vector<uint8_t> & buffer)
	{
		vConsumeUpdate(client, buffer.data(), buffer.size());

        auto blob = vProduceResponse(client);
        std::vector<uint8_t> response(vGetBlobSize(blob));
        memcpy(response.data(), vGetBlobData(blob), response.size());
        vFreeBlob(blob);
        return response;
	}
};

int main(int argc, char * argv []) try
{
	Server server;
	Client client0,client1;

	if (glfwInit() != GL_TRUE) throw std::runtime_error("glfwInit() failed.");
	auto win = glfwCreateWindow(1280, 720, "Simple Simulation", nullptr, nullptr);
	if (!win) throw std::runtime_error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);
    
    std::mt19937 engine;
    std::uniform_real_distribution<float> dist;

	double t0 = glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
		glfwPollEvents();

		const double t1 = glfwGetTime(), timestep = t1 - t0;
		if (timestep < 1.0 / 60) continue;
		t0 = t1;

		server.Update(static_cast<float>(timestep));

		auto buffer0 = server.ProduceUpdate(0);
        auto buffer1 = server.ProduceUpdate(1);
        if(dist(engine) > 0.2f) // Client 0 has a crappy connection, and drops packets frequently
        {
		    auto response0 = client0.Update(buffer0);
            if(dist(engine) > 0.2f) server.ConsumeResponse(0, response0);
        }
        if(dist(engine) > 0.2f)
        {
            auto response1 = client1.Update(buffer1);
            // Client 1 never responds, to prove that we can still transfer game state and our memory usage is bounded
        }
        
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
        server.Draw();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
		client0.Draw(1,0,0);	
        client1.Draw(0,1,1);
        glDisable(GL_BLEND);
        glPopMatrix();
		glfwSwapBuffers(win);

        std::cout << server.GetMemUsage() << " " << client0.GetMemUsage() << " " << client1.GetMemUsage() << " " << buffer0.size() << " " << buffer1.size() << std::endl;
	}

	glfwDestroyWindow(win);
	glfwTerminate();
	return EXIT_SUCCESS;
}
catch (const std::exception & e)
{
	std::cerr << "Caught exception: " << e.what() << std::endl;
	glfwTerminate();
	return EXIT_FAILURE;
}