%% Parameter direct solve
zeta_min   = 0.7;
ts_target  = 0.3;      % seconds -- adjust to spec
k_max      = 500e3;    % N/m -- worst case

omega_n = 4 / (zeta_min * ts_target);
m_v     = k_max / omega_n^2;
b       = 2 * zeta_min * sqrt(k_max * m_v);

fprintf('m_v = %.3f kg\n', m_v);
fprintf('b   = %.3f N*s/m\n', b);

%% Verify: peak force under F_max even at worst case
F_step = 2000;  % N
overshoot_frac = exp(-zeta_min*pi/sqrt(1-zeta_min^2));
F_peak_estimate = F_step * (1 + overshoot_frac);
fprintf('Estimated peak force: %.1f N (limit: 4760 N, %.0f%% margin)\n', ...
    F_peak_estimate, 100*(1 - F_peak_estimate/4760));