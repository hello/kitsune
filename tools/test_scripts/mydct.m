function[cx,rotvec]=mydct(x)
%%x = some row vector
flip = 0;
if size(x,2) == 1
    x = x';
    flip = 1;
end


xx = [ x fliplr(x) ];

yy = fft(xx);

y = yy(1:length(xx)/2);
n = length(x);
k = 0:n-1;
rotvec = exp(-pi*k*1i / 2 / n);

cx = real(rotvec .* y);

q = 1 / sqrt(n) / sqrt(2);
cx = cx * q;

if flip
    cx = cx';
end
