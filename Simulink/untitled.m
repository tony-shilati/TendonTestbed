
k = 100;
m_v = 1;
b = 5;

% System model
sys = tf(k*b, [m_v, b, k*b]);

step(sys)

% stepinfo (plot how the characteristics change with parameters)

for k = 50e3:500e3
    for m_v = 
        for b = 