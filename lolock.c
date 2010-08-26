/************************************************************************
 *                                                                      *
 * Copyright (c) 2010 Sylvain `Magicking' Laurent <syllaur@gmail.com>   *
 *                                                                      *
 * lolock is free software: you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * lolock is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with lolock.  If not, see <http://www.gnu.org/licenses/>.      *
 *                                                                      *
 ************************************************************************/

#include <SDL.h>
#include <SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <security/pam_appl.h>

#include "font.h"

#define ERROR(msg,type) {\
                         fprintf(stderr, "\nError in %s:%d\n%s: %s\n", \
                         __FILE__, __LINE__, msg, type##_GetError()); \
                         exit(1); \
                       }

#define FONT_SIZE 42
#define PASS_MAX_SIZE 24
#define REPLACEMENT_CHAR '#'


static int          cursor_state;
static SDL_Surface *sdlsScreen = NULL;
static TTF_Font    *ttffFont   = NULL;
static char        *login_str;

void release_ressource()
{
  free(login_str);
  SDL_ShowCursor(cursor_state);
  TTF_Quit();
  SDL_FreeSurface(sdlsScreen);
  SDL_Quit();
}

static TTF_Font * Load_Font(int ptsize)
{
  SDL_RWops *sdlrwmem = NULL;

  sdlrwmem = SDL_RWFromConstMem(&FontDump, FontSize);

  return TTF_OpenFontRW(sdlrwmem, 1, ptsize);
}

int conv_func(int num_msg, const struct pam_message **msg,
              struct pam_response **resp, void *appdata_ptr)
{
  int                  i;

  if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
    return PAM_CONV_ERR;

  if (!(*resp = calloc(num_msg, sizeof (**resp))))
    return PAM_BUF_ERR;

  for (i = 0; i < num_msg; i++) //Should never be > 1
  {
    (*resp[i]).resp_retcode = 0;
    if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF)
      (*resp[i]).resp = strdup((const char *)appdata_ptr);
    else
      (*resp[i]).resp = NULL;
  }

  return PAM_SUCCESS;
}

static int check_pw(const char *login, const char *pwd)
{
  struct pam_conv  pam_c;
  pam_handle_t    *phth;
  int              ret;

  pam_c.conv = conv_func;
  pam_c.appdata_ptr = (void *)pwd;

  assert(pam_start("lolock", login, &pam_c, &phth) == PAM_SUCCESS);

  ret = pam_authenticate(phth, PAM_SILENT);

  pam_end(phth, 0);
  return ret == PAM_SUCCESS;
}

static void init(SDL_Surface ** sdlsScreen, TTF_Font ** ttffFont)
{
  SDL_putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    ERROR ("Could not initialize SDL",SDL);

  if (TTF_Init() < 0)
    ERROR ("Could not initialize TTF",TTF);

  atexit(release_ressource);

  if ((*ttffFont = Load_Font(FONT_SIZE)) == NULL)
    ERROR ("Could not open font", TTF);

  if ((*sdlsScreen = SDL_SetVideoMode(0, 0, 32,
    SDL_SWSURFACE | SDL_FULLSCREEN )) == NULL)
    ERROR ("Could not initialize video mode",SDL);

  cursor_state = SDL_ShowCursor(SDL_QUERY);
  SDL_ShowCursor(SDL_DISABLE);

  SDL_EnableUNICODE(1);
}

static int ProcessPassword(char * str, int size)
{
  (void)size;

  return check_pw(login_str, str);
}

static void ProcessPending(char * str, int size)
{
  SDL_Rect       sdlrDest;
  SDL_Surface   *sdlsText   = NULL;
  SDL_Color      sdlcWhite  = {255, 255, 255, 0};
  char          *buffer     = NULL;
  int            height;
  int            i;

  (void)str;

  if (size > 0)
  {
    buffer = (char*)malloc(size + 1);
    for (i = 0 ; i < size; i++)
      buffer[i] = REPLACEMENT_CHAR;
    buffer[i] = 0;

      sdlsText = TTF_RenderUTF8_Blended(ttffFont, buffer, sdlcWhite);

    free(buffer);
  }
  height = TTF_FontHeight(ttffFont);

  sdlrDest.x = 0;
  sdlrDest.y = (sdlsScreen->h / 2) - (height / 2) + (height + 8);
  sdlrDest.w = sdlsScreen->w;
  sdlrDest.h = height;
  SDL_FillRect(sdlsScreen, &sdlrDest, 0);
  if (size > 0)
  {
    sdlrDest.x = (sdlsScreen->w / 2) - (sdlsText->w / 2);
    sdlrDest.w = sdlsText->w;
    SDL_BlitSurface(sdlsText, NULL, sdlsScreen, &sdlrDest);
    SDL_FreeSurface(sdlsText);
  }
  sdlrDest.x = 0;
  sdlrDest.w = sdlsScreen->w;
  SDL_UpdateRects(sdlsScreen, 1, &sdlrDest);
}

int main()
{
  SDL_Surface   *sdlsText   = NULL;
  SDL_Color      sdlcWhite  = {255, 255, 255, 0};
  SDL_Rect       sdlrDest;
  SDL_Event      sdleEvt;

  char          *login_str_up;
  int            login_len;
  char          *msg_print;
  const char    *msg = "THIS SCREEN IS LOCKED BY ";

  int            password_len = 0;
  char           password[PASS_MAX_SIZE + 1];

  int            i;

  init(&sdlsScreen, &ttffFont);

  login_str = strdup(getpwuid(getuid())->pw_name);
  login_len = strlen(login_str);
  login_str_up = (char*)malloc(login_len + 1);

  for (i = 0; i <= login_len; i++)
    login_str_up[i] = toupper(login_str[i]);

  msg_print = (char*)malloc(strlen(msg) + login_len + 1);
  sprintf(msg_print, "%s%s", msg, login_str_up);

  sdlsText = TTF_RenderUTF8_Blended(ttffFont, msg_print, sdlcWhite);

  free(msg_print);
  free(login_str_up);
  sdlrDest.x = (sdlsScreen->w / 2) - (sdlsText->w / 2);
  sdlrDest.y = (sdlsScreen->h / 2) - (sdlsText->h / 2) - (FONT_SIZE + 8);
  sdlrDest.w = sdlsText->w;
  sdlrDest.h = sdlsText->h;

  SDL_BlitSurface(sdlsText, NULL, sdlsScreen, &sdlrDest);
  SDL_UpdateRects(sdlsScreen, 1, &sdlrDest);

  SDL_FreeSurface(sdlsText);

  SDL_WM_GrabInput(SDL_GRAB_ON);

  while (SDL_WaitEvent(&sdleEvt))
    if (sdleEvt.type == SDL_KEYDOWN && sdleEvt.key.keysym.unicode & 0x07F)
      switch (sdleEvt.key.keysym.unicode & 0x07F)
      {
        case 0x0D: //Enter
          password[password_len] = 0;
          if (ProcessPassword(password, password_len))
            exit(0);
          else
            ProcessPending(NULL, 0);
          password_len = 0;
          while (SDL_PollEvent(&sdleEvt));
          break;
        case 0x01b:
          password_len = 0;
        case 0x08: //backspace
          if (password_len)
            password_len--;
          ProcessPending(password, password_len);
          break;
        default:
          if (password_len >= PASS_MAX_SIZE)
            break;
          password[password_len] = sdleEvt.key.keysym.unicode & 0x07F;
          password_len++;
          ProcessPending(password, password_len);
          break;
      }

  return 0;
}
