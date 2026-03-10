#pragma once
#include <chrono>
#include <thread>

class Timer
{
private:
    uint32_t m_frameCounter;
    double m_targetFrameDuration;
    double m_oneSecAccumulator = 0.0;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_frameStartTime;

public:

    Timer(uint32_t targetFPS) : m_frameCounter(0), m_targetFrameDuration(1.0/targetFPS), m_oneSecAccumulator(0.0)
    {
    }

    void Sleep(double milliSeconds)
    {
        std::this_thread::sleep_for(std::chrono::duration<double>(milliSeconds));
    }

    void StartFrame()
    {
        m_frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void EndFrame()
    {
        m_frameCounter++;

        std::chrono::duration<double> elapsedTime = std::chrono::high_resolution_clock::now() - m_frameStartTime;

        auto CalculateWait = [this, elapsedTime]()
        {
            m_oneSecAccumulator += elapsedTime.count();
            if (elapsedTime.count() < m_targetFrameDuration)
            {
                // check if we need to wait
                // within one second window on per frame basis check if we will reach the target frame count
                if (m_oneSecAccumulator < m_frameCounter * m_targetFrameDuration)
                {
                    auto sleepDuration = m_frameCounter * m_targetFrameDuration - m_oneSecAccumulator;
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleepDuration));
                    m_oneSecAccumulator += sleepDuration;
                }
            }
        };

        auto ResetCounters = [this]()
        {
            if (m_oneSecAccumulator >= 1.0)
            {
                //std::cout << "1 : fps : " << m_frameCounter << "\n";
                m_oneSecAccumulator = m_oneSecAccumulator - 1.0;
                m_frameCounter = 0;
            }
            else
            {
                // check if another frame is possible
                if (m_oneSecAccumulator + m_targetFrameDuration > 1.0)
                {
                    // if not possible sleep till one sec duration gets over
                    std::this_thread::sleep_for(std::chrono::duration<double>(1.0 - m_oneSecAccumulator));
                    //std::cout << "2 : fps : " << m_frameCounter << "\n";
                    m_oneSecAccumulator = 0.0;
                    m_frameCounter = 0;
                }
            }
        };

        auto additionalWorkStartTimePoint = std::chrono::high_resolution_clock::now();

        CalculateWait();
        ResetCounters();

        std::chrono::duration<double> extraTime = std::chrono::high_resolution_clock::now() - additionalWorkStartTimePoint;
        m_oneSecAccumulator += extraTime.count();
    }
};