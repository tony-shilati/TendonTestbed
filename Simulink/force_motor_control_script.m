%% setup_model.m
% Defines parameters and runs the Simulink model

%% Set up the workspace
clear; 
clc; 
close all;

% Define parameters
m_v = 1378.125;        % virtual mass, kg
b = 36750;      % damping, N*s/m
k = 500e3;    % tendon stiffness, N/m (200 N/mm)
% k_range = (50:50:500)*1e3;   % N/m, sweeping 50–500 N/mm

% Simulation settings
Tsim = 10;       % simulation stop time, seconds

% Open the model
open_system('force_motor_control');

%% Run the simulation
simOut = sim('force_motor_control', 'StopTime', num2str(Tsim));

%% Plot results
figure;
plot(simOut.tout, simOut.yout{1}.Values.Data);
xlabel('Time (s)');
ylabel('F (N)');
title('Step Response');
grid on;