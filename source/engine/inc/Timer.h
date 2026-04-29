#ifndef TIMER_H
#define TIMER_H

#include <plog/Log.h>
#include <chrono>
#include <thread>
#include <string>
#include <functional>

namespace Loops
{
    class Timer
    {
    private:
        uint32_t m_frameCounter;
        double m_targetFrameDuration;
        double m_oneSecAccumulator = 0.0;
        float m_deltaTime = 0.0;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_frameStartTime;

    public:

        explicit Timer(uint32_t targetFPS) : m_frameCounter(0), m_targetFrameDuration(1.0 / targetFPS), m_oneSecAccumulator(0.0)
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
            m_deltaTime = elapsedTime.count() + extraTime.count();
        }

        const double& GetDeltaTime() const
        {
            return m_deltaTime;
        }
    };

    class Benchmark
    {
    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
        std::string m_targetName;
        bool m_deactivated = false;

        std::vector<std::function<void(const std::string&)>> m_onExit;

    public:
        explicit Benchmark(const std::string& targetName) : m_targetName(targetName)
        {
            m_startTime = std::chrono::high_resolution_clock::now();
        }

        explicit Benchmark(const std::string& targetName, const std::vector<std::function<void(const std::string&)>>& onExit) :
            m_targetName(targetName), m_onExit(onExit)
        {
            m_startTime = std::chrono::high_resolution_clock::now();
        }

        void Stop()
        {
            std::chrono::duration<double> elapsedTime = std::chrono::high_resolution_clock::now() - m_startTime;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime);
            auto s = std::chrono::duration_cast<std::chrono::seconds>(elapsedTime);

            // Using ostringstream for formatting
            std::ostringstream oss;
            oss << "Time taken for " << m_targetName << " : " << ms;
            std::string output{ oss.str() };
            PLOGD << output;

            m_deactivated = true;
            for (auto& func : m_onExit)
            {
                func(output);
            }
        }

        ~Benchmark()
        {
            if (!m_deactivated)
            {
                Stop();
            }
        }
    };
}
#endif // !TIMER_H
