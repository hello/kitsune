%q is the fixed point representation, if it's left out, then this will output floating point
function printMatrix(x,name,q,fid)
isFloat = false;

if (nargin < 3)
  isFloat = true;
else
  if isempty(q)
    isFloat = true;
  end
end

if (nargin < 4)
  isFile = false;
  fid = stdout;
else
  if isempty(fid)
    isFile = false;
    fid = stdout;
  else
    isFile=true;
    fid = fopen(fid,'w');
  end
end

%make a vector always be a row vector
sx = size(x);
if sx(2) == 1
  x = x';
  sx = size(x);
end


fprintf(fid,'%s',name)
if any(sx == 1)
  %it's a vector
  isVector = true;
  fprintf(fid,'[%d] = ',sx(2))
else
  isVector = false;
  fprintf(fid,'[%d][%d] = {',sx(1),sx(2))
end

for j = 1:sx(1)
  fprintf(fid,'{')

  for i = 1:sx(2)
    if isFloat
      fprintf(fid,'%f',x(j,i))
    else
      fprintf(fid,'%d',round(x(j,i)*2^q))
    end

    if i ~= sx(2)
      fprintf(fid,',')
    end
  end

  fprintf(fid,'}')
  if (j ~= sx(2))
    fprintf(fid,',\n')
  end
end

if (~isVector)
  fprintf(fid,'}')
end

if isFile
  fclose(fid)
end
