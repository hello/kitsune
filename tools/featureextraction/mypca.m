function [y,d] = mypca(x)
%takes row data
mx = mean(x);
x2 = x - repmat(mx,size(x,1),1);

P = x2'*x2;
d = diag(P);
d = sqrt(d);
N = length(P);
for j = 1:N
  for i = 1:N
     P(j,i) = P(j,i) / d(i) / d(j);
  end
end



[eigvec,eigvals] = eig(P);
d = diag(eigvals);
d = d / sum(d);

y = x * eigvec;
E = eigvec;
