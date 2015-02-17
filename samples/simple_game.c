/* Copyright (c) 2015 Sterling Orsten
     This software is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software. You are granted a perpetual, 
   irrevocable, world-wide license to copy, modify, and redistribute
   this software for any purpose, including commercial applications. */

#include <netcode.h>
#include <netcodex.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Protocol data */
NCprotocol * protocol;
NCclass * unitClass, * deathEvent;
NCint * teamField, * hpField, * xField, * yField, * targetField;
NCint * deathX, * deathY;

/* Server state */
struct Unit
{
    int team, hp;
    float x, y;
    NCobject * object;
} units[20];
NCauthority * serverAuth;
NCpeer * serverPeer;

/* Client state */
NCauthority * clientAuth;
NCpeer * clientPeer;
GLFWwindow * win;

void error(const char * message)
{
    fprintf(stderr, message);
    exit(EXIT_FAILURE);
}

void spawn_unit(int i)
{
    units[i].team = i < 10 ? 0 : 1;
    units[i].hp = 100;
    units[i].x = (float)(rand() % 320 + units[i].team * 960);
    units[i].y = (float)(rand() % 720);
    units[i].object = ncCreateObject(serverAuth, unitClass);
}

int main(int argc, char * argv[])
{
    int i, j, n, x, y, h; float a, t0, t1, timestep; 
    NCblob * updateBlob, * responseBlob;

    /* init protocol */
    protocol = ncCreateProtocol(30);
    unitClass = ncCreateClass(protocol, 0);
    teamField = ncCreateInt(unitClass, NC_CONST_FIELD_FLAG);
    hpField = ncCreateInt(unitClass, 0);
    xField = ncCreateInt(unitClass, 0);
    yField = ncCreateInt(unitClass, 0);
    targetField = ncCreateRef(unitClass);
    deathEvent = ncCreateClass(protocol, NC_EVENT_CLASS_FLAG);
    deathX = ncCreateInt(deathEvent, NC_CONST_FIELD_FLAG);
    deathY = ncCreateInt(deathEvent, NC_CONST_FIELD_FLAG);

    /* init server */
    serverAuth = ncCreateAuthority(protocol);
    for(i=0; i<20; ++i) spawn_unit(i);
    serverPeer = ncCreatePeer(serverAuth);
    
    /* init client */
    clientAuth = ncCreateAuthority(protocol);
    clientPeer = ncCreatePeer(clientAuth);
    if (glfwInit() != GL_TRUE) error("glfwInit() failed.");
	win = glfwCreateWindow(1280, 720, "Simple Game", NULL, NULL);
	if (!win) error("glfwCreateWindow(...) failed.");
	glfwMakeContextCurrent(win);

    /* main loop */
	t0 = (float)glfwGetTime();
	while (!glfwWindowShouldClose(win))
	{
    	t1 = (float)glfwGetTime(), timestep = t1 - t0;
		if (timestep < 1.0 / 60) continue;
		t0 = t1;

        /* for each game unit on server */
        for(i=0; i<20; ++i)
        {
            /* select a target */
            int target=0; float dx, dy, dist, best=40000000;
            for(j=0; j<20; ++j)
            {
                if(units[i].team == units[j].team) continue;
                dx = units[j].x - units[i].x;
                dy = units[j].y - units[i].y;
                dist = dx*dx + dy*dy;
                if(dist < best)
                {
                    target = j;
                    best = dist;
                }
            }

            /* pursue target */
            dx = units[target].x - units[i].x;
            dy = units[target].y - units[i].y;
            dist = sqrtf(dx*dx + dy*dy);
            if(dist > 50) /* if far, move towards target */
            {
                units[i].x += dx * 50 * timestep / dist;
                units[i].y += dy * 50 * timestep / dist;
            }
            else /* if near, attack target */
            {
                --units[target].hp;
                if(units[target].hp < 1) /* if target is dead, destroy and respawn */
                {
                    NCobject * e = ncCreateObject(serverAuth, deathEvent);
                    ncSetObjectInt(e, deathX, units[target].x);
                    ncSetObjectInt(e, deathY, units[target].y);
                    ncSetVisibility(serverPeer, e, 1);

                    ncDestroyObject(units[target].object);
                    spawn_unit(target);
                }
            }
            
            /* update corresponding netcode object */
            ncSetObjectInt(units[i].object, teamField, units[i].team);
            ncSetObjectInt(units[i].object, hpField, units[i].hp);
            ncSetObjectInt(units[i].object, xField, (int)units[i].x);
            ncSetObjectInt(units[i].object, yField, (int)units[i].y);
            ncSetObjectRef(units[i].object, targetField, units[target].object);
            ncSetVisibility(serverPeer, units[i].object, 1); /* for now, all units are always visible, but we could implement a "fog of war" by manipulating this */
        }
        ncPublishFrame(serverAuth);

        /* simulate network traffic */
        updateBlob = ncProduceMessage(serverPeer);
        if(rand() % 100 > 20) /* simulate 20% packet loss, in a real app, blob would be transmitted from server to client via UDP */
        {
            ncConsumeMessage(clientPeer, ncGetBlobData(updateBlob), ncGetBlobSize(updateBlob));
            
            ncPublishFrame(clientAuth);
            responseBlob = ncProduceMessage(clientPeer);
            if(rand() % 100 > 20) /* simulate 20% packet loss, in a real app, blob would be transmitted from client to server via UDP */
            {
                ncConsumeMessage(serverPeer, ncGetBlobData(responseBlob), ncGetBlobSize(responseBlob));
            }
            ncFreeBlob(responseBlob);
        }
        printf("Update size: %d B\n", ncGetBlobSize(updateBlob));
        ncFreeBlob(updateBlob);

        /* redraw client */        
		glClear(GL_COLOR_BUFFER_BIT);
		glPushMatrix();
		glOrtho(0, 1280, 720, 0, -1, +1);
        for(i=0, n=ncGetViewCount(clientPeer); i<n; ++i)
        {
            const NCview * view = ncGetView(clientPeer, i), * view2;
            if(ncGetViewClass(view) == unitClass)
            {
                /* draw colored circle to represent unit */
                x = ncGetViewInt(view, xField);
                y = ncGetViewInt(view, yField);
                
                if(view2 = ncGetViewRef(view, targetField))
                {
                    int x2 = ncGetViewInt(view2, xField), y2 = ncGetViewInt(view2, yField);
                    glColor3f(1,1,1);
                    glBegin(GL_LINES);
                    glVertex2i(x,y);
                    glVertex2i(x2,y2);
                    glEnd();
                }

                glBegin(GL_TRIANGLE_FAN);
                switch(ncGetViewInt(view, teamField))
                {
                case 0: glColor3f(0,1,1); break;
                case 1: glColor3f(1,0,0); break;
                }
                for(j=0; j<12; ++j)
                {
                    a = 6.28f * j / 12;
                    glVertex2f(x + cosf(a)*10, y + sinf(a)*10);
                }
                glEnd();
                
                /* draw unit health bar */
                h = ncGetViewInt(view, hpField);
                glBegin(GL_QUADS);
                glColor3f(0,1,0);
                glVertex2i(x-10, y-13);
                glVertex2i(x-10+(h*20/100), y-13);
                glVertex2i(x-10+(h*20/100), y-11);
                glVertex2i(x-10, y-11);
                glColor3f(0.5f,0.2f,0);
                glVertex2i(x-10+(h*20/100), y-13);
                glVertex2i(x+10, y-13);
                glVertex2i(x+10, y-11);
                glVertex2i(x-10+(h*20/100), y-11);
                glEnd();
            }
            else if(ncGetViewClass(view) == deathEvent)
            {
                x = ncGetViewInt(view, deathX);
                y = ncGetViewInt(view, deathY);
                glBegin(GL_TRIANGLE_FAN);
                glColor3f(1,1,0);
                for(j=0; j<12; ++j)
                {
                    a = 6.28f * j / 12;
                    glVertex2f(x + cosf(a)*20, y + sinf(a)*20);
                }
                glEnd();
            }
        }
        glPopMatrix();
		glfwSwapBuffers(win);

        glfwPollEvents();
	}

    ncxPrintCodeEfficiency(clientPeer);

	glfwDestroyWindow(win);
	glfwTerminate();
    return EXIT_SUCCESS;
}