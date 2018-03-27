%%
clear;
clc;
samples1 = importdata('test.txt');
Fs = 44100;
soundsc(samples1,Fs);

%%
clear;
clc;
samples1 = importdata('test.txt');
samples2 = importdata('test2.txt');

for i = 1:length(samples1)
    if samples1(i)  ~= 0
        samples1(i)
        i
        break
    end
end

for i = 1:length(samples2)
    if samples2(i)  ~= 0
        samples2(i)
        i
        break;
    end
end
%% Pitch detection algorithm 

n = 16384*16:16384*32;
N = 16384*32-16384*16;
FS = 44100;
han_window = 0.5*(1-cos(2*pi*n)/(1024-1));
data = samples1(n)';
windowed_signal = data.*han_window;
freq = abs(fft(windowed_signal));
plot(abs(freq));
max = 0;
index = 0;
for i = 1:length(freq)/2
    if freq(i) > max
        index = i-1;
        max = freq(i);
    end
end
f = index *(FS/N);


