function v = btkGetAnglesValues(h) %#ok
%BTKGETANGLESVALUES Extract angles' coordinates and store them in a matrix
% 
%  V = BTKGETANGLESVALUES(H) returns a matrix of real.  The number of rows
%  of this matrix corresponds to the number of frames in the acquisition.  The number of
%  columns of this matrix corresponds to the number of extracted angles 
%  multiplied by 3 (their coordinates X,Y,Z).
%  The biomechanical acquisition handle H is represented as a 32-bit integer
%  and can be only used with the btk* functions.

%  Author: A. Barré
%  Copyright 2009-2010 Biomechanical ToolKit (BTK).

% The following comment, MATLAB compiler pragma, is necessary to avoid 
% compiling this M-file instead of linking against the MEX-file.  Don't remove.
%# mex

error(generatemsgid('NotSupported'),'MEX file for BTKGETANGLESVALUES not found');

% [EOF] btkGetAnglesValues.m
