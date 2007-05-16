/*
  File:       iccExtractValuesFromGrabbedProbeImage.cpp
 
  Contains:   Command-line app that takes the pathname of a screen grab
  of a probe image, and the pixels coordinates within that image
  of the white border of the content area, and extracts from the
  pixels within the border area the non-ICC-color-managed values
  of the (known) probe pixel values, establishing a relationship
  between that non-ICC system's un-color-managed and color-managed
  pixel values.
 
  At the moment this code is specific to Mac OS X 10.4 and up.
  Contributions of counterpart code for linux and/or Windows
  would be very seriously considered for inclusion here, including
  cross-platform versions using libtiff.
 
  Version:    V1
 
  Copyright:  � see below
*/

/*
 * The ICC Software License, Version 0.1
 *
 *
 * Copyright (c) 2003-2006 The International Color Consortium. All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:  
 *       "This product includes software developed by the
 *        The International Color Consortium (www.color.org)"
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "ICC" and "The International Color Consortium" must
 *    not be used to imply that the ICC organization endorses or
 *    promotes products derived from this software without prior
 *    written permission. For written permission, please see
 *    <http://www.color.org/>.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium. 
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes. 
 *
 *  
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *  
 * 
 */

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Joseph Goldstone spring 2007
//
//////////////////////////////////////////////////////////////////////

#include <iostream>
using namespace std;

#include "Vetters.h"

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

CGColorSpaceRef
getDeviceRGBColorSpace()
{
  static CGColorSpaceRef deviceRGB = NULL;
  if (deviceRGB == NULL)
    deviceRGB = CGColorSpaceCreateDeviceRGB();
  return deviceRGB;
}

// from Gelphman & Laden, pp. 390-391, Listing 12.10
CGColorSpaceRef
getTheDisplayColorSpace()
{
  static CGColorSpaceRef displayCS = NULL;
  if (displayCS == NULL)
  {
    CMProfileRef displayProfile = NULL;
    // Get the display ID of the main display.
    
    // For displays other than the main display, use
    // the functions CGGetDisplaysWithPoint,
    // CGGetDisplaysWithRect, etc. in CGDirectDisplay.h
    CGDirectDisplayID displayID = CGMainDisplayID();
    // The CGDirectDisplayID is the same as the CMDisplayIDType
    // passed to CMGetProfileByAVID
    CMError err = CMGetProfileByAVID((CMDisplayIDType)displayID,
                                     &displayProfile);
    if (err || displayProfile == NULL)
    {
      fprintf(stderr, "Got error %d when gettingprofile for main display!\n",
              err);
      return NULL;
    }
    
    displayCS = CGColorSpaceCreateWithPlatformColorSpace(displayProfile);
    CMCloseProfile(displayProfile);
  }
  return displayCS;
}

// best would be...

// CGColorSpaceRef
// getTheGrabbedImageColorSpace()
// {
//   ...
// }

// ..but one thing at a time.

#define BEST_BYTE_ALIGNMENT 16
#define COMPUTE_BEST_BYTES_PER_ROW(bpr)\
( ( (bpr) + (BEST_BYTE_ALIGNMENT-1) ) & ~(BEST_BYTE_ALIGNMENT-1) )

void
usage(ostream& s, const char* const myName)
{
  s << myName << ": usage is " << myName << " image extracted_data N"
       << " white_left white_right white_bottom white_top\n"
  << " where\n"
  << "image is the pathname of the file containing the grabbed probe"
  << " frame\n"
  << "extracted_data is the pathname to which the extracted data will"
  << " be written\n"
  << "N is the number of samples per edge of the probe cube\n"
  << "white_left is the pixel coordinate, relative to the lower left corner"
  << " of the image being (0, 0), of the leftmost content pixel\n"
  << "white_right is the pixel coordinate, relative to the red line"
  << " around the image, of the rightmost content pixel\n"
  << "white_bottom is the pixel coordinate, relative to the red line"
  << " around the image, of the bottommost content pixel\n"
  << "white_top is the pixel coordinate, relative to the red line"
  << " around the image, of the topmost content pixel\n"
  << "\n"
  << "example:\n"
  << " extract_probe_data grabbed_frame.tiff extracted_data.txt 52"
  << " 2 565 2 285"
  << endl;
}

int
main(int argc, const char * argv[]) {
  const char* const my_name = path_tail(argv[0]);
  if (argc != 8)
  {
    usage(cout, my_name);
    return EXIT_FAILURE;
  }
  const char* const inputImagePath = argv[1];
  vet_input_file_pathname(inputImagePath, "image", "the pathname of a file"
                          " containing the grabbed image of the probe frame");
  
  const char* const outputTextPath = argv[2];
  vet_output_file_pathname(outputTextPath, "extracted_data", "the pathanme of a"
                           " file to which the data extracted from the image"
                           " will be written");
  
  const char* const NChars = argv[3];
  vet_as_int(NChars, "N", "the number of samples per edge of the probe cube,"
             " typically 52");
  int  N = atoi(NChars);
  
  const char* const whiteLeftChars = argv[4];
  vet_as_int(whiteLeftChars, "white_left", "an integer which is the offset"
             " from the lower left pixel (at 0,0) of the leftmost pixel in"
             " the content area");
  int whiteLeft = atoi(whiteLeftChars);

  const char* const whiteRightChars = argv[5];
  vet_as_int(whiteRightChars, "white_right", "an integer which is the offset"
             " from the lower left pixel (at 0,0) of the rightmost pixel in"
             " the content area");
  int whiteRight = atoi(whiteRightChars);
  
  const char* const whiteBottomChars = argv[6];
  vet_as_int(whiteBottomChars, "white_bottom", "an integer which is the offset"
             " from the lower left pixel (at 0,0) of the bottommost pixel in"
             " the content area");
  int whiteBottom = atoi(whiteBottomChars);
  
  const char* const whiteTopChars = argv[7];
  vet_as_int(whiteTopChars, "white_top", "an integer which is the offset"
             " from the lower left pixel (at 0,0) of the topmost pixel in"
             " the content area");
  int whiteTop = atoi(whiteTopChars);
  
  // first arg is name of probe frame file
  // if it's not there, error out.
  // from this compute minimum number of pixels required to contain data.
  // and from that, compute (via sqrt) min run size for edge
  
  // next, read in the image.  (does this have a failure mode?  Don't think so.)
  CFStringRef URLString = CFStringCreateWithCString(NULL,
                                                    inputImagePath,
                                                    kCFStringEncodingASCII);
  
  //  CFShow(URLString);
  //  CFShowStr(URLString);
  
  CFURLRef URL = CFURLCreateWithFileSystemPath(NULL, URLString,
                                               kCFURLPOSIXPathStyle, false);
  if (URL == NULL)
  {
    fprintf(stderr, "Can't create URL.\n");
    return EXIT_FAILURE;
  }
  
  CGImageSourceRef imageSource = CGImageSourceCreateWithURL(URL, NULL);
  if (imageSource == NULL)
  {
    fprintf(stderr, "Couldn't create image source from URL.\n");
    return EXIT_FAILURE;
  }
  CGImageRef fullImage = CGImageSourceCreateImageAtIndex(imageSource, 0, NULL);
  if (fullImage == NULL)
  {
    fprintf(stderr, "Couldn't create full image from image source.\n");
    return EXIT_FAILURE;
  }
  
  size_t fullImageWidth  = CGImageGetWidth(fullImage);
  size_t fullImageHeight = CGImageGetHeight(fullImage);
  size_t bitsPerComponent = CGImageGetBitsPerComponent(fullImage);
  CGBitmapInfo bitmapInfo = CGImageGetBitmapInfo(fullImage);

  // This is in the Photoshop or Shake-style coordinate system with
  // the origin at the lower left.  According to Gelphman & Laden's
  // "Programming with Quartz", p. 349 bottom, this is also the initial
  // coordinate system of a bitmap context.
  size_t contentLeft = whiteLeft + 2;
  size_t contentRight = whiteRight - 2;
  size_t contentBottom = whiteBottom + 2;
  size_t contentTop = whiteTop - 2;
  size_t contentWidth = (contentRight - contentLeft) + 1;
  size_t contentHeight = (contentTop - contentBottom) + 1;

  size_t fullImageBytesPerRow;
  switch (bitmapInfo)
  {
    case kCGImageAlphaNone:
    case kCGImageAlphaNoneSkipFirst:
    case kCGImageAlphaNoneSkipLast:
    case kCGImageAlphaPremultipliedFirst:
    case kCGImageAlphaPremultipliedLast:
      fullImageBytesPerRow = COMPUTE_BEST_BYTES_PER_ROW(4 * fullImageWidth);
      break;
    case kCGImageAlphaNone | kCGBitmapFloatComponents:
    case kCGImageAlphaNoneSkipFirst | kCGBitmapFloatComponents:
    case kCGImageAlphaNoneSkipLast | kCGBitmapFloatComponents:
    case kCGImageAlphaPremultipliedFirst | kCGBitmapFloatComponents:
    case kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents:
      fullImageBytesPerRow = COMPUTE_BEST_BYTES_PER_ROW(16 * fullImageHeight);
      break;
    default:
      fprintf(stderr, "Unknown bitmap info - unable to figure out optimal bytes per full row.\n");
  }
    
  double maxValue = (1 << bitsPerComponent) - 1.0;
  fprintf(stderr, "full width %d, full height %d, bpc %d, full bytes per row %d, maxValue %f, contentWidth %d, contentHeight %d\n",
          fullImageWidth, fullImageHeight,
          bitsPerComponent, fullImageBytesPerRow, maxValue,
          contentWidth, contentHeight);
  
  //  CGColorSpaceRef colorSpace = getDeviceRGBColorSpace();
  CGColorSpaceRef colorSpace = getTheDisplayColorSpace();
  if (colorSpace == NULL)
  {
    //    fprintf(stderr, "Couldn't get device color space.\n");
    fprintf(stderr, "Couldn't get display color space.\n");
    return EXIT_FAILURE;
  }
  
  
  unsigned char* data
    = static_cast<unsigned char*>(calloc(1, fullImageBytesPerRow * fullImageHeight));
  
  CGContextRef bitmapContext
    = CGBitmapContextCreate(data, fullImageWidth, fullImageHeight,
                            bitsPerComponent, fullImageBytesPerRow, colorSpace,
                            kCGImageAlphaNoneSkipLast);
  if (bitmapContext == NULL)
  {
    fprintf(stderr, "Can't create bitmap context from image attributes.\n");
    return EXIT_FAILURE;
  }
  
  CGRect fullImageRect = CGRectMake(0, 0, fullImageWidth, fullImageHeight);
  CGContextDrawImage(bitmapContext, fullImageRect, fullImage);

  FILE* file = fopen(outputTextPath, "w");
  if (file == NULL)
  {
    fprintf(stderr, "error: %s: %s\n", my_name, strerror(errno));
    return EXIT_FAILURE;
  }
  
  int x = contentLeft;
  int y = contentBottom;
  int r;
  int g;
  int b;
  for (r = 0; r < N; ++r)
    for (g = 0; g < N; ++g)
      for (b = 0; b < N; ++b)
      {
        unsigned char* row = data + ((fullImageHeight - 1) - y) * fullImageBytesPerRow;
        unsigned char* pixel = row + x * 4;
        fprintf(file, "%f %f %f\n",
                *(pixel + 0) / maxValue,
                *(pixel + 1) / maxValue,
                *(pixel + 2) / maxValue);
        ++x;
        if (x > contentRight)
        {
          x = contentLeft;
          ++y;
        }
      }

  int closeStatus = fclose(file);
  if (closeStatus != 0)
  {
    fprintf(stderr, "%s: error: %s.\n", my_name, strerror(errno));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}