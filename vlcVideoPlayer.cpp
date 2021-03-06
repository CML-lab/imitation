#include "vlcVideoPlayer.h"


void DrawOtherStuff(void (*func)(SDL_Renderer *renderer))
{
	 drawfunc = func;
}


// VLC prepares to render a video frame.
void* lock(void *data, void **p_pixels) {

    struct ctx *c = (ctx *)data;

    int pitch;
    SDL_LockMutex(c->mutex);
    SDL_LockTexture(c->texture, NULL, p_pixels, &pitch);

    return NULL; // Picture identifier, not needed here.
}


// VLC just rendered a video frame.
void unlock(void *data, void *id, void *const *p_pixels) {

    struct ctx *c = (ctx *)data;

    uint16_t *pixels = (uint16_t *)*p_pixels;

    // We can also render stuff.
	/*
    int x, y;
    for(y = 10; y < 40; y++) {
        for(x = 10; x < 40; x++) {
            if(x < 13 || y < 13 || x > 36 || y > 36) {
                pixels[y * VIDEOWIDTH + x] = 0xffff;
            } else {
                // RV16 = 5+6+5 pixels per color, BGR.
                pixels[y * VIDEOWIDTH + x] = 0x02ff;
            }
        }
    }
	*/

    SDL_UnlockTexture(c->texture);
    SDL_UnlockMutex(c->mutex);
}

// VLC wants to display a video frame.
void display(void *data, void *id) {

    struct ctx *c = (ctx *)data;

    //c->rect.w = VIDEOWIDTH;
    //c->rect.h = VIDEOHEIGHT;
	//c->rect.x = c->xpos - c->rect.w/2;
	//c->rect.y = c->ypos - c->rect.h/2;

	if (c->showVideo)
	{
		SDL_ShowWindow(c->window);
		SDL_SetRenderDrawColor(c->renderer, 255, 255, 255, 255);
		SDL_RenderClear(c->renderer);
		SDL_RenderCopy(c->renderer, c->texture, NULL, &(c->rect));
		
		if (drawfunc  != NULL)
			drawfunc(c->renderer);
		/*SDL_Rect rect;
		rect.w = SCREEN_WIDTH/2;
		rect.h = SCREEN_HEIGHT/12;
		rect.x = SCREEN_WIDTH/2 - rect.w/2;
		rect.y = SCREEN_WIDTH/2 - rect.h/2;
	
		SDL_SetRenderDrawColor(c->renderer, 255, 0, 0, 255);
		//SDL_RenderClear(c->renderer);
		SDL_RenderDrawRect(c->renderer, &rect);
		*/
	
		SDL_RenderPresent(c->renderer);
	}
	else
	{
		SDL_SetRenderDrawColor(c->renderer, 255, 255, 255, 255);
		SDL_RenderClear(c->renderer);
		SDL_HideWindow(c->window);
	}
}




Video::Video(const char* fname, int x, int y, int w, int h, int* errorcode) //SDL_Renderer *renderer, SDL_Texture *texture, SDL_mutex *mutex, SDL_Rect rect)
{
	//Note, this constructor does the "bad" thing of potentially failing, in which case it does not return a valid initialized object.
	//We need an external way to catch when this happens. To do so, we will pass it an "errorcode" that it will write into before it exits.
	//Then we can look at the value of the error code after calling the constructor. If the error code is 0, the constructor succeeded;
	//otherwise, the value of the error code indicates at what stage the initialization failed.

	*errorcode = 0;

	isValid = 1;

	//we need absolute paths, so we must figure out the project directory
	char *bpath = SDL_GetBasePath();
	std::string basepath;
	basepath.assign(bpath);
	//std::cerr << "BasePath: " << basepath.c_str() << std::endl;
	basepath.erase(basepath.rfind("\\"),1); //get rid of the last slash in the path
	basepath.erase(basepath.rfind("\\")+1,10); //get rid of the "Debug" folder name to get to the project folder
	//std::cerr << "ModBasePath: " << basepath.c_str() << std::endl;


    //Set the environmental variable to point to the VLC plugins directory
    //   If this isn't set properly, libvlc_new() will not work!
	//NOTE: Sometimes VLC still won't initialize even with this workaround; if libvlc_new() returns NULL,
	//      just drop a copy of the VLC plugins folder into the Debug folder.
	std::string envpathcmd;
	envpathcmd.assign("VLC_PLUGIN_PATH=");
	envpathcmd.append(basepath.c_str());
	envpathcmd.append("vlc-3.0.6\\plugins");
	_putenv(envpathcmd.c_str());
	std::cerr << "VLC_PLUGIN_PATH=" << getenv("VLC_PLUGIN_PATH") << std::endl;	
    //printf("VLC_PLUGIN_PATH=%s\n", getenv("VLC_PLUGIN_PATH"));


	//we will set up a dedicated window and renderer for the video, as it plays asynchronously
	context.window = SDL_CreateWindow(
            "Vidplayer",
            x-w/2,
            y-h/2,
            w, h,
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR); // 
    if (!context.window)
	{
        std::cerr << "Couldn't create window: " << SDL_GetError() << std::endl;
		*errorcode = 1;
		isValid = 0;
		//return;
	}

    context.renderer = SDL_CreateRenderer(context.window, -1, 0);
    if (!context.renderer){
        std::cerr << "Couldn't create renderer: " << SDL_GetError() << std::endl;
		*errorcode = 2;
		isValid = 0;
		//return;
	}

	//set the window position in absolute pixel space on the screen
	context.xpos = x;
	context.ypos = y;

	//create a texture to put the video in, which is the same size as the window
	context.texture = SDL_CreateTexture(
            context.renderer,
            SDL_PIXELFORMAT_BGR565, SDL_TEXTUREACCESS_STREAMING,
            w, h);
    if (!context.texture) {
        std::cerr << "Couldn't create texture: " << SDL_GetError() << std::endl;
		*errorcode = 3;
		isValid = 0;
		//return;
    }

    context.mutex = SDL_CreateMutex();

	//set the video frame to fill the texture/window
	context.rect.w = w;
	context.rect.h = h;
	context.rect.x = 0;
	context.rect.y = 0;
	
	//set up some libVLC initialization parameters
	
	char const *vlc_argv[] = {
        "--no-audio", // Don't play audio.
        "--no-xlib", // Don't use Xlib.
        // Apply a video filter.
        //"--video-filter", "sepia",
        //"--sepia-intensity=200"
    };
    int vlc_argc;
	

	//initialize the special drawfunc to be null
	drawfunc = NULL;
	
	vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);
	//std::cout << vlc_argc << " - " << vlc_argv[0] << " " << vlc_argv[1] << std::endl;
	libvlc = libvlc_new(vlc_argc, vlc_argv);
	
	// Initialise libVLC.
    //libvlc = libvlc_new(0, NULL);
    if(libvlc == NULL) {
		std::cerr << "LibVLC initialization failure." << std::endl;
		*errorcode = 4;
		isValid = 0;
		return;
		//exit;
        //return EXIT_FAILURE;
    }

	//set up the video file to be played
	//libVLC wants an absolute path, so we will figure out the project directory
	std::stringstream vidpath;
	int d = 0;
	vidpath << basepath.c_str() << VIDEOPATH << fname; //"\\Video" << d << ".divx";
	std::cerr << "VidPath: " << vidpath.str().c_str() << std::endl;

	//check if the path exists. For some reason libVLC doesn't do this check correctly so we have to do it manually!
	if (!PathFileExistsA(vidpath.str().c_str()))
	{
		std::cerr << "Video file/path does not exist." << std::endl;
		*errorcode = 5;
		isValid = 0;
		return;
	}

	//open the video file
	m = libvlc_media_new_path(libvlc, vidpath.str().c_str());
	if (m == NULL)
	{
		std::cerr << "Media path not valid." << std::endl;
		*errorcode = 6;
		isValid = 0;
		return;
	}
	//else
		//std::cerr << "Media path: " << m << std::endl;

    mp = libvlc_media_player_new_from_media(m);
	if (mp == NULL)
	{
		std::cerr << "Media Player not created." << std::endl;
		*errorcode = 7;
		isValid = 0;
		return;
	}

    libvlc_media_release(m);

	libvlc_video_set_scale(mp,1.5);		

    libvlc_video_set_callbacks(mp, lock, unlock, display, &context);

    libvlc_video_set_format(mp, "RV16", w, h, w*2);

	mpevent = libvlc_media_player_event_manager(mp);
	ResetStatus();
	GetStatus();

	context.showVideo = 0;
	
	isVisible = 1;

	Invisible();  //make the window invisible until we need it

	std::cerr << "Video: " << vidpath.str().c_str() << " load complete: status = " << *errorcode << "." << std::endl;

}

void Video::SetPos(int x, int y)
{
	context.xpos = x;
	context.ypos = y;

}


void Video::SetValidStatus(int status)
{
	isValid = status;
}

int Video::IsValid()
{
	return(isValid);
}


int Video::GetStatus()
{

	float pos;

	if (!isValid)
		return(-1);

	//get the status of the video, and update some status flags
	mpstate = libvlc_media_get_state(m);

	/*
	if (mpstate == libvlc_Playing)
	{
		pos = libvlc_media_player_get_position(mp);
		//std::cerr << "Video position: " << pos << " : " << libvlc_media_player_get_time(mp) << std::endl;	
	}
	else
		pos = -1; //we did not query the pos state
	
	else if (mpstate == libvlc_Ended)
	{
		pos = 1.0;
	}
	else
	{
		//if the state is stopped, we have to briefly play it to get the position
		libvlc_media_player_play(mp);  //resetting the position only works if the video is playing...
		pos = libvlc_media_player_get_position(mp);
		libvlc_media_player_stop(mp);
		//libvlc_media_player_set_time(mp,0);
	}
	*/

	if (hasStarted == 0 && (mpstate == libvlc_Playing))
	{
		hasStarted = 1;
		hasEnded = 0;
	}
	else if (hasStarted == 1 && ((mpstate == libvlc_Stopped && (vidPos-1.0)<1e-3) || mpstate == libvlc_Ended) ) //(mpstate == libvlc_Stopped && (pos-1.0)<1e-3) || )
	{
		hasEnded = 1;
		//hasStarted = 0; //we will not clear this until requested separately
	}
	else if (hasStarted == 1 && (mpstate == libvlc_Stopped || mpstate == libvlc_Paused) )
	{
		hasStopped = 1;
	}

	//std::cerr << "  Video Play Status: " << mpstate << " : (" << hasStarted << "," << hasEnded << ")." << std::endl;
	
	return(mpstate);
}

int Video::HasStarted()
{
	if (!isValid)
		return(-1);

	GetStatus();

	return(hasStarted);
}

int Video::HasEnded()
{
	if (!isValid)
		return(-1);

	GetStatus();

	return(hasEnded);
}


int Video::HasStopped()
{
	if (!isValid)
		return(-1);

	GetStatus();

	return(hasStopped);
}

void Video::ResetStatus()
{
	hasStarted = 0;
	hasEnded = 0;
	hasStopped = 0;

	std::cerr << "Vid status flags reset" << std::endl;
}


int Video::Play()
{
	//returns 0 if playback started.

	int status = 1;
	
	if (!isValid)
		return(-1);

	context.showVideo = 1;
	Visible();

	GetStatus();

	std::cerr << "State: " << libvlc_media_get_state(m) << std::endl;

	//if the screen is not visible, sometimes it takes a little for the window to appear
	//this causes the beginning of the video to cut off
	//we need to ask for a delay before playing the video
	
	//std::cerr << "Vid vis: " << isVisible << std::endl;

	if (mpstate == libvlc_Ended)
	{
		status = libvlc_media_player_play(mp);
		libvlc_media_player_set_position(mp,0.0f); //reset the position so we can play again
		vidPos = libvlc_media_player_get_position(mp);
		std::cerr << "Video play from reset position: " << vidPos  << std::endl;  //<< " : " << libvlc_media_player_get_time(mp)
		
		GetStatus();

	}
	else if ((mpstate != libvlc_Playing)) // && ((SDL_GetTicks() - VisTime) > 20))
	{
		//start playing the video
		status = libvlc_media_player_play(mp);
		std::cerr << "Play video." << std::endl;

		//VidIsPlaying = 1;
		//libvlc_event_attach(mpevent,libvlc_MediaPlayerEndReached,videoEnded,VidIsPlaying); //set up a callback to detect video end

		visTime = SDL_GetTicks();

		GetStatus();

		vidPos = libvlc_media_player_get_position(mp);

	}
	else if (mpstate == libvlc_Playing)
	{
		status = 1;
		vidPos = libvlc_media_player_get_position(mp);
	}
	else
		status = 0;

	std::cerr << "VidPlay State: " << libvlc_media_get_state(m) << std::endl;


	return(status);
}

int Video::Stop()
{
	int status = 0;

	if (!isValid)
		return(-1);

	context.showVideo = 0;
	//Invisible();

	GetStatus();
	
	if (!(mpstate == libvlc_Ended) && !(mpstate == libvlc_Stopped) ) //&& !(mpstate == libvlc_Paused)
	{
		std::cerr << "Video stop requested...";
		//libvlc_media_player_stop(mp);
		//libvlc_media_player_pause(mp);
		libvlc_media_player_stop(mp);
		std::cerr << " done." << std::endl;
		status = 1;

		GetStatus();
	
	}

	return(status);
}

int Video::Pause()
{

	if (!isValid)
		return(-1);

	int status = 0;
	context.showVideo = 0;

	libvlc_media_player_pause(mp);
	std::cerr << "Video pause requested." << std::endl;

	GetStatus();
	if (mpstate == libvlc_Paused)
		status = 1;

	return(status);

}

int Video::ResetVid()
{
	if (!isValid)
		return(-1);

	int status = 0;
	
	//libvlc_media_player_play(mp);  //resetting the position only works if the video is playing...

	GetStatus();

	if (mpstate == libvlc_Stopped)
	{
		libvlc_media_player_play(mp); //if the video is stopped, we have to play it so that we can reset the position.
		libvlc_media_player_set_position(mp,0.0f);
		//libvlc_media_player_set_time(mp,0);
		vidPos = libvlc_media_player_get_position(mp);
		std::cerr << "Video position: " << vidPos  << std::endl;  //<< " : " << libvlc_media_player_get_time(mp)
		if ((vidPos - 0.0f) < 1e-4)
			status = 1;
		//libvlc_media_player_pause(mp);
		libvlc_media_player_stop(mp);
	}
	else if (mpstate == libvlc_Ended)
	{
		//from the ended state, resetting the position can cause problems...
		//we ask to stop the video to change its state, then we can play and reset its position.

		libvlc_media_player_stop(mp);
		libvlc_media_player_play(mp);
		libvlc_media_player_set_position(mp,0.0f);
		vidPos = libvlc_media_player_get_position(mp);
		libvlc_media_player_stop(mp);
		std::cerr << "Video position: " << vidPos  << std::endl;  //<< " : " << libvlc_media_player_get_time(mp)
		if ((vidPos - 0.0f) < 1e-4)
			status = 1;
		
	}
	else if (mpstate == libvlc_Playing)
	{
		libvlc_media_player_set_position(mp,0.0f);
		vidPos = libvlc_media_player_get_position(mp);
		std::cerr << "Video position: " << vidPos  << std::endl;  //<< " : " << libvlc_media_player_get_time(mp)
		if ((vidPos - 0.0f) < 1e-4)
			status = 1;
		libvlc_media_player_stop(mp);
		
	}
	else if (mpstate == libvlc_Paused)
	{
		//libvlc_media_player_play(mp);  //from the paused state, it seems like we can reset the position without first playing
		libvlc_media_player_set_position(mp,0.0f);
		vidPos = libvlc_media_player_get_position(mp);
		std::cerr << "Video position: " << vidPos  << std::endl;  //<< " : " << libvlc_media_player_get_time(mp)
		if ((vidPos - 0.0f) < 1e-4)
			status = 1;
		libvlc_media_player_stop(mp);
	}

	std::cerr << "Video State: " << libvlc_media_get_state(m) << std::endl;

	ResetStatus();

	return(status);
}


void Video::Visible()
{
	
	Uint32 winFlags;

	SDL_ShowWindow(context.window);
	winFlags = SDL_GetWindowFlags(context.window);
	if (winFlags & SDL_WINDOW_SHOWN && ~isVisible)
	{
		std::cerr << "Vid window is visible." << std::endl;
		visTime = SDL_GetTicks();
		isVisible = 1;
	}
	
}

void Video::Invisible()
{
	Uint32 winFlags;

	SDL_HideWindow(context.window);
	winFlags = SDL_GetWindowFlags(context.window);
	if (winFlags & SDL_WINDOW_HIDDEN)
	{
		std::cerr << "Vid window is hidden." << std::endl;
		isVisible = 0;
	}
	
}



void Video::CleanUp()
{
	if (isValid > 0)
	{
		
		// Stop stream and clean up libVLC.
		libvlc_media_player_stop(mp);
		libvlc_media_player_release(mp);
	}
	libvlc_release(libvlc);

	// Close window and clean up libSDL.
    SDL_DestroyMutex(context.mutex);
    SDL_DestroyRenderer(context.renderer);
	SDL_DestroyWindow(context.window);

}



/*
//callback to detect when the video has stopped playing
int VidIsPlaying;
void videoEnded(const libvlc_event_t *event, void *vidPlaying)
{
	std::cerr << "VideoEnd callback (" << vidPlaying << "):(";
	*vidPlaying = 0;
	std::cerr << vidPlaying << "):  ";

}
*/
