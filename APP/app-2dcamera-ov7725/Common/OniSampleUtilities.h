/*****************************************************************************
*                                                                            *
*  OpenNI 2.x Alpha                                                          *
*  Copyright (C) 2012 PrimeSense Ltd.                                        *
*                                                                            *
*  This file is part of OpenNI.                                              *
*                                                                            *
*  Licensed under the Apache License, Version 2.0 (the "License");           *
*  you may not use this file except in compliance with the License.          *
*  You may obtain a copy of the License at                                   *
*                                                                            *
*      http://www.apache.org/licenses/LICENSE-2.0                            *
*                                                                            *
*  Unless required by applicable law or agreed to in writing, software       *
*  distributed under the License is distributed on an "AS IS" BASIS,         *
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
*  See the License for the specific language governing permissions and       *
*  limitations under the License.                                            *
*                                                                            *
*****************************************************************************/
#ifndef ONISAMPLEUTILITIES_H
#define ONISAMPLEUTILITIES_H

#include <stdio.h>
#include <OpenNI.h>

#ifdef WIN32
#include <conio.h>
int wasKeyboardHit(void);

#else // linux

#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
int wasKeyboardHit(void);
void Sleep(int millisecs);
#endif // WIN32

void calculateHistogram(float* pHistogram, int histogramSize, const openni::VideoFrameRef& frame);

#endif // ONISAMPLEUTILITIES_H
